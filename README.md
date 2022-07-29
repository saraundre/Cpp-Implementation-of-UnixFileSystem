# UnixFileSystem-CppImplementation


--------------------------
Requirements
--------------------------

1．	Review Unix file system design and i-node usage.
2．	Design and implement an i-node-based Unix-style file system.
3．	Implement basic functionalities specified in the following section.
4．	The task needs to be completed using C++ ONLY 


--------------------------
Tasks/Functionalities
--------------------------

The following functions are required in your file system: 
1．	Allocate 16MB space in memory as the storage for your file system. The space is divided into blocks with block size 1KB 
Assume address length is 24-bit，please design your virtual address structure.
Design what information should be contained in an i-node
The i-node should support 10 direct block addresses, and one indirect block address

2．	The first a few blocks can be used for storing the i-nodes, and the first i-node can be used for the root directory (/). 
	(You can design the structure as you like, as long as it is reasonable and well explained in your report.)

3．	Using random strings to fill the files you created. It means you just need to specify the file size (in KB) and path+name.
4．	Following commands should be supported in your system：
a)	A welcome message with the group info (names and IDs) when the system is launched. It is also the claim of your ‘copyright’
b)	Create a file：createFile fileName fileSize  
i.e.：createFile /dir1/myFile 10 (in KB)
if fileSize > max file size, print out an error message.

c)	Delete a file：deleteFile filename 
i.e.：deleteFile /dir1/myFile

d)	Create a directory：createDir 
i.e.：createDir /dir1/sub1 (should support nested directory)

e)	Delete a directory：deleteDir 
i.e.:	deleteDir /dir1/sub1（The current working directory is not allowed to be deleted）

f)	Change current working directory：changeDir 
i.e.: changeDir /dir2 

g)	List all the files and sub-directories under current working directory：dir 
You also need to list at least two file attributes. (i.e., file size, time created, etc.)

h)	Copy a file: cp 
i.e.: cp file1 file2

i)	Display the usage of storage space：sum  
Display the usage of the 16MB space. You need to list how many blocks are used and how many blocks are unused. 

j)	Print out the file contents: cat  
Print out the contents of the file on the terminal
i.	e:  cat /dir1/file1
