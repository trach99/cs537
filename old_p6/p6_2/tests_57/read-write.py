#!/usr/bin/python3

# write data to numfiles and read it back
# we do small writes to ensure block alignment works

import os
import sys

numfiles = int(sys.argv[1])
filelist = ["file" + str(n + 1) for n in range(numfiles)]
numwrites = int(sys.argv[2])
segment = 100

data = os.urandom(segment * numwrites)

os.chdir("mnt")

# write all the data in segments
fhs = [open(name, "wb") for name in filelist]

for i in range(numwrites):
    for fh in fhs:
        towrite = data[i * segment:(i + 1) * segment]
        fh.write(towrite)

for fh in fhs:
    fh.close()

# read each file back and compare to original data
for name in filelist:
    with open(name, "rb") as file:
        contents = file.read()
        if not data == contents:
            print(f"{name} readback does not match data written")
            chunk_size = 512  # Define chunk size
            length1 = len(data)  # Total length of the array
            length2 = len(contents)
            print("data written : ")
            for start in range(0, length1, chunk_size):
                end = min(start + chunk_size, length1)  # Ensure it doesn't go past the end
                print(f"Bytes {start}-{end-1}: {data[start:end]}\n")
            print("\n\n\ndata read : ")
            for start in range(0, length2, chunk_size):
                end = min(start + chunk_size, length2)  # Ensure it doesn't go past the end
                print(f"Bytes {start}-{end-1}: {data[start:end]}\n")
            exit(1)
            
print("Correct")
exit(0)

#####################################################################################################

# #!/usr/bin/python3

# # write data to numfiles and read it back
# # we do small writes to ensure block alignment works

# import os
# import sys

# numfiles = int(sys.argv[1])
# filelist = ["file" + str(n + 1) for n in range(numfiles)]
# numwrites = int(sys.argv[2])
# segment = 100

# data = os.urandom(segment * numwrites)

# os.chdir("mnt")

# # write all the data in segments
# fhs = [open(name, "wb") for name in filelist]

# print("Writing data...")
# for i in range(numwrites):
#     for fh in fhs:
#         towrite = data[i * segment:(i + 1) * segment]
#         # print(f"Writing segment {i + 1}: {towrite[:50]}...")  # Print first 50 bytes for brevity
#         fh.write(towrite)

# for fh in fhs:
#     fh.close()

# # read each file back and compare to original data
# print("\nReading back data and comparing...")
# for name in filelist:
#     with open(name, "rb") as file:
#         contents = file.read()
#         if not data == contents:
#             print(f"{name} readback test does not match data written")
#             print(f"Data written : {data[0:50]}...\n\n")
#             print(f"Data read : {contents[0:50]}...\n")
#             exit(1)

# print("Success")
# exit(0)


