#! /bin/env python

import toolspath
from testing import Xv6Build, Xv6Test


class test1(Xv6Test):
    name = "test_1"
    description = "Checks the presence of all system calls"
    tester = "ctests/test_1.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test2(Xv6Test):
    name = "test_2"
    description = "Checks the presence of Segmentation Fault"
    tester = "ctests/test_2.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "Segmentation Fault"
    failure_pattern = "PASSED"


class test3(Xv6Test):
    name = "test_3"
    description = "MAP: Place one fixed anonymous map"
    tester = "ctests/test_3.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test4(Xv6Test):
    name = "test_4"
    description = "MAP: Place one fixed filebacked map"
    tester = "ctests/test_4.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test5(Xv6Test):
    name = "test_5"
    description = "MAP: Place multiple maps, verify that overlapping maps are not allowed"
    tester = "ctests/test_5.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test6(Xv6Test):
    name = "test_6"
    description = "MAP+ALLOC: Access fixed anonymous map (checks for memory allocation)"
    tester = "ctests/test_6.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test7(Xv6Test):
    name = "test_7"
    description = "MAP+ALLOC: access big filebacked map (checks for memory allocation)"
    tester = "ctests/test_7.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test8(Xv6Test):
    name = "test_8"
    description = "MAP+STRESS: Places a large number of maps and accesses all pages of each map"
    tester = "ctests/test_8.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test9(Xv6Test):
    name = "test_9"
    description = "MAP+LAZY: Checks for lazy allocation in anon mapping"
    tester = "ctests/test_9.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test10(Xv6Test):
    name = "test_10"
    description = "MAP+LAZY: Checks for lazy allocation in filebacked mapping"
    tester = "ctests/test_10.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test11(Xv6Test):
    name = "test_11"
    description = "MAP+LAZY+STRESS: Checks for lazy allocation in filebacked mapping"
    tester = "ctests/test_11.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test12(Xv6Test):
    name = "test_12"
    description = "UNMAP: Unmap a anonymous map"
    tester = "ctests/test_12.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test13(Xv6Test):
    name = "test_13"
    description = "UNMAP: Unmaps a filebacked map"
    tester = "ctests/test_13.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test14(Xv6Test):
    name = "test_14"
    description = "UNMAP+DEALLOC: Unmap accessed anonymous map and check for memory deallocation"
    tester = "ctests/test_14.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test15(Xv6Test):
    name = "test_15"
    description = "UNMAP+DEALLOC: Unmap an accessed filebacked map and check for memory deallocation"
    tester = "ctests/test_15.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test16(Xv6Test):
    name = "test_16"
    description = "UNMAP: Edit a filebacked map and verify its changes are reflected"
    tester = "ctests/test_16.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test17(Xv6Test):
    name = "test_17"
    description = "Fork: Same maps exist in both parent and multiple childs"
    tester = "ctests/test_17.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test18(Xv6Test):
    name = "test_18"
    description = "Fork: Same map contents exist in both parent and multiple childs"
    tester = "ctests/test_18.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test19(Xv6Test):
    name = "test_19"
    description = "Fork+EDIT: Both parent and child can see each other's edits in anonymous maps"
    tester = "ctests/test_19.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test20(Xv6Test):
    name = "test_20"
    description = "FORK+UNMAP: Child unmaps a shared map, parent is not affected"
    tester = "ctests/test_20.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test21(Xv6Test):
    name = "test_21"
    description = "ELF: fix permissions of ELF pages"
    tester = "ctests/test_21.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "Segmentation Fault"
    failure_pattern = "PASSED"


class test22(Xv6Test):
    name = "test_22"
    description = "COW: allocated array has same pa in parent and child before modification"    
    tester = "ctests/test_22.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test23(Xv6Test):
    name = "test_23"
    description = "COW: allocated array has diff pa in parent and child after modification"
    tester = "ctests/test_23.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test24(Xv6Test):
    name = "test_24"
    description = "COW: kfreeing child's page does not affect parent's page in COW"
    tester = "ctests/test_24.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test25(Xv6Test):
    name = "test_25"
    description = "COW+STRESS: static parent arr 50000 pages, can't fork child without COW"
    tester = "ctests/test_25.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 1
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


from testing.runtests import main

main(
    Xv6Build,
    all_tests=[
        test1,
        test2,
        test3,
        test4,
        test5,
        test6,
        test7,
        test8,
        test9,
        test10,
        test11,
        test12,
        test13,
        test14,
        test15,
        test16,
        test17,
        test18,
        test19,
        test20,
        test21,
        test22,
        test23,
        test24,
        test25,
    ],
    # Add your test groups here
    # End of test groups
)
