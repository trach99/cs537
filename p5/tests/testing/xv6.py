import shutil, os, subprocess
import re
from testing import Test, BuildTest, pexpect


class Xv6Test(BuildTest, Test):
    name = "all"
    description = "build xv6 using make"
    timeout = 30
    tester = "tester.c"
    header = None # Fariha: Added header file
    make_qemu_args = ""
    point_value = 0
    success_pattern = "SUCCESS"
    failure_pattern = "FAILED"

    def __call__(self):
        return run(self)

    def run(self):
        tester_path = self.test_path + "/" + self.tester
        self.log("Running xv6 user progam " + str(tester_path))
        shutil.copy(tester_path, self.project_path + "/tester.c")
        if self.header: # Fariha: Copy the header file to the project path
            print("DEBUG: Copying header file to project path")
            shutil.copy(self.test_path + "/" + self.header, self.project_path + "/tester.h") 


        # shawgerj copy Makefile to Makefile.test and gawk tester.c into UPROGS
        #      cmd = "gawk '($1 == \"_mkdir\\\") { printf(\"\t_tester\\\n\"); } { print $0 }'"
        cmd = [
            self.test_path + "/edit-makefile.sh",
            self.project_path + "/Makefile",
            self.project_path + "/Makefile.test",
        ]

        # with open(self.test_path + "/Makefile", "r") as m:
        #    with open(self.project_path + "/Makefile.test", "w+") as mtest:
        #       subprocess.Popen(cmd, stdin=m, stdout=mtest, shell=True)

        subprocess.Popen(cmd)
        is_success = self.make(["xv6.img", "fs.img"])
        if not is_success:
            return  # stop test on if make fails

        target = "qemu-nox " + self.make_qemu_args
        if self.use_gdb:
            target = "qemu-gdb " + self.make_qemu_args
        self.log("make " + target)
        child = pexpect.spawn(
            "make -f Makefile.test " + target,
            cwd=self.project_path,
            logfile=self.logfd,
            timeout=None,
        )
        self.children.append(child)

        if self.use_gdb:
            gdb_child = subprocess.Popen(
                ["xterm", "-title", '"gdb"', "-e", "gdb"], cwd=self.project_path
            )
            self.children.append(gdb_child)

        child.expect_exact("init: starting sh", timeout=self.timeout)
        child.expect_exact("$ ", timeout=self.timeout)
        child.sendline("tester")
        child.expect_exact("tester", timeout=self.timeout)
        index = child.expect([r"(.*)\$ ", "panic"], timeout=self.timeout)
        # Extract the text between "tester" and "$"/"panic"
        captured_text = child.match.group(1).strip()

        # Define patterns and their corresponding actions
        SUCCESS_ACTION = "success"
        patterns = {
            "FAILED": "tester failed",
            " panic": "xv6 kernel panic",
            "--kill proc": "killed process",
            "Segmentation Fault": "segmentation fault",
            self.failure_pattern: "tester failed",
            self.success_pattern: SUCCESS_ACTION, # "success"
        }
        # If in some cases, panic or kill proc is expected to be a success
        for pattern in patterns.keys():
            if pattern == self.success_pattern:
                patterns[pattern] = SUCCESS_ACTION
        # If in some cases, SUCCESS or PASSED is expected to be a failure
        for pattern in patterns.keys():
            if pattern == self.failure_pattern:
                patterns[pattern] = "tester failed"

        # print("DEBUG: " + str(patterns))

        # Find all matches in the captured text
        matched_actions = [action for pattern, action in patterns.items() if re.search(pattern, captured_text)]
        # If all match is SUCCESS_ACTION, then the test passed
        # Otherwise, the test failed
        if  len(matched_actions) > 0 and matched_actions.count(SUCCESS_ACTION) == len(matched_actions):
            self.done()
        else:
            self.fail(matched_actions[0] if matched_actions else "tester failed")

        if self.use_gdb:
            child.wait()
        else:
            child.close()


class Xv6Build(BuildTest):
    name = "build"
    description = "build xv6 using make"
    timeout = 60
    targets = ["xv6.img", "fs.img"]

    def __call__(self):
        return self.done()
