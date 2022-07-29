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



--------------------------
Project Report
--------------------------
[Background]
Unix file system is a logical method of organizing and storing large amounts of information. The greatest advantage of this file system is that it makes it easy to manage. In Unix, everything is considered a file, including devices such as DVD-ROMS, USB devices, and floppy drives. In addition, a file is the smallest unit in which the information is stored. All files are organized into directories and these directories are organized into a tree-like structure called the file system.

Files in Unix systems are organized into a multi-level hierarchy structure known as a directory tree. At the very top of the file, the system is a directory called “root” which is represented by a “/”. The root contains other files and directories. Each file or directory is uniquely identified by its name, the directory in which it resides, and a unique identifier called I-node. Finally, a Unix file system is self-contained. There are no dependencies between one filesystem and another.

Early versions of Unix filesystems were referred to simply as FS. FS only included the boot block, superblock, a clump of I-node, and the data blocks. This worked well for the small disks early Unix operating systems were designed for. However, as technology advanced and disks grew larger, moving the head back and forth between the clump of I-node and the data blocks they referred to caused thrashing. 

To solve this issue, Marshall Kirk McKusick optimized the BSD 4.2's FFS (Fast File System) by inventing cylinder groups, which break the disk up into smaller chunks, with each group having its respective I-node and data blocks. The intent of BSD FFS was to try to localize associated data blocks and metadata in the same cylinder group. Thus, it would reduce fragmentation caused by scattering a directory's contents over a whole disk.


[Project design]
In this experiment, we first wrote pseudocode, which we referred to as we code along. The pseudocode is shown in the following sections with its respective functions. For the memory allocation, 16MB of memory is allocated to the file system as storage. This space is further divided into blocks of 1KB size, resulting in a maximum of 16,384 blocks. Following a small file system standard, our inode is 128 bytes in size. Further, the cylindrical group is partitioned into Superblock, Inode Bitmap, Block Bitmap, Inode space, and Data space. In the first block, the Superblock contains filesystem metadata, which describes the organization of the filesystem and how much disk space is being used. 

![image](https://user-images.githubusercontent.com/110232966/181746751-2ff2a76e-4efa-48cd-8e4a-99fe4c83a7d2.png)

The I-node table/space contains the metadata of the file and it is used to map to the data blocks using pointers. It is composed of 10 direct pointers, which point to blocks of the file's data, and 1 indirect pointer, which points to an inode without information headers before pointing to data blocks. 
I-node bitmaps occupy a part of the memory and are a sequence of 0s and 1s where 0 indicates a free I-node and 1 indicates an I-node that is already in use. This technique is used to quickly find the free location on the inode table when modifying the file system. On-disk these sections can be illustrated as follows.
 
 ![image](https://user-images.githubusercontent.com/110232966/181746806-f6426518-116c-4874-86d5-49d757a5d267.png)

Similarly, in the block bitmap, each bit represents a specific block and indicates whether it is available or not. After the file system allocation, all pending space goes into the data space, which contains the raw data stored in the blocks.

Assuming that the address length is 24 bits and that the inode supports 10 direct block addresses and one indirect block address, the addressing structure can be designed and calculated as such:


----------------------------------------------------------------------------------------------------------------

Max file size = No of direct disk block pointer + (DBS/DBA) + (DBS/DBA) ^2 + (DBS/DBA) ^3+ (DBS/DBA) ^N) *DBS

DBS = Disk Block size= 1024 bytes = 1KB
DBA = Disk Block address size = 24 bits = 3 bytes

The number of disk block pointer that can fit in one block pointer = DBS/DBA = 1024/3 = 341.3333 = 341

The number of block 10 direct address entries can address = 10 
The number of block one primary indirect block can address = 341
Max file size = (341+10) *1KB = (351) * 1024 = 359424 bytes

----------------------------------------------------------------------------------------------------------------



[Implementation]
We first define the header files and some constant variables as shown in figure1.1. In this program, we allocate 1024 bytes as block size, and 16384 bytes as block number. This maintains the 16MB storage capacity of the file system based on BLOCK_NUM x BLOCK_SIZE. Considering we are implementing a small file system, INODE_SIZE is assigned 128 bytes as per standards, and the INODE_NUM is 1024.

The Flag bit indicates if the object is a file (0) or a directory (1) and is used to initialize the number of direct blocks in the inode, as well as maximum values for the name, file size, and the number of directories. 

![image](https://user-images.githubusercontent.com/110232966/181747042-6ed173bf-e65a-4111-b213-4f9bf4848457.png)

The program consists mainly of two parts: the data structure and the functions. The three main data structures defined are Superblock, inode, and DirItem as shown in the figure. The superblock struct contains the disk usage data and the block’s start address. Inode structs contain information such as unique inodeID, file sizes, types, number of items, and direct and indirect block addresses. The file types include directories and files. The dirItem struct contains the name of a directory item, which can either be a file or a subdirectory, as well as the inodeAddress of the item.
 
Then we initialize the objects for the superblock of the filesystem and the file stream so that we can create, read, and write data to the file. This is followed by the initialization of block and inode bitmaps, current directory path and address, and global variables 

The functions are divided into the following subsections according to their functionalities so they can be easily understood and read.
o	Allocation 
o	Update Functions
o	Get Addresses 
o	Block Functions 
o	Check Functions
o	Formatting & Disk usage 
o	Directory Functions 
o	Current Directory Functions 
o	Remove Functions
o	File Functions




 	Allocation 
This section defines the ialloc () and balloc () functions as shown in the figure1.4. The inode allocation function assigns an inode to a newly created file, whereas the block allocation function assigns a block to the data of the newly created file.
If the function is unable to allocate successfully, it returns -1 otherwise it returns the inode/block address.


 	Update Functions 
In this section, the functions update the inode bitmap, the block bitmap, and the super bitmap as shown in figure1.5. It utilizes the file stream methods; seekp () and write (). In the seekp () method, the pointer is set to the start of the inode/block bitmap address and the stream is modified from the beginning, and in the write () method, the value of the inode/block bitmap is inserted into the constant pointer to the inode bitmap address. The updateSuperblock () works similarly


    Get Addresses  
This section contains three functions; getDirAddr (), getPathInodeAddr (), and getBlockAddr () which return the location of our directory/file so that the system can find them based on user input as shown in the figure1.6.
The getDirAddr () returns the directory item address using either the direct or indirect addressing. If the path cannot be explored further, getPathInodeAddr () returns the inode path address for the current working directory, if path == "/" it returns the root. We iterate over the path size if the path is not root and if the global variables for the respective functions are set to true, we execute an if statement that returns the address as per the file type.  The getBlockAddr () maps to the block address from the current inode. 


    Block Functions 
A block address can be read and written using the bwrite () and bprint () functions.

 	Check functions 
The isvalid () function checks if the file and the directory name are valid by checking if the given characters are alphanumeric or not. If it is, then it returns true or else false. Whereas the check () function checks whether the given path is in the “/string/” format



 	Formatting & disk usage 
The formatting of the filesystem path, disk, and data can be achieved using the functions listed in the figure1.9. The formatpath () function is used to return the passed path string according to the system default. In order to maintain the integrity of the database, every file system must be able to restore the original state of the files (default state), so the format () function is defined to delete all files and directories within the database.

The load () function loads the previously loaded memory and facilitates the loading of the superblock, inode, and block bitmaps. This loads the data from the previous run, which is called on program execution to update the database if any changes were made prior to running the system.
 
 The printDiskSpaceUsage () functions prints the Inode and block usage on the disk by the filesystem. It includes the total number of inode/blocks on disk, the number of free inodes/blocks on disk, and the number of used inode/blocks by the filesystem.  
 
 
 
 	Directory functions 
The Unix filesystem is initially configured with a root directory "/", it is the first and topmost directory in the filesystem hierarchy. The program facilitates easy navigation and manipulation of the file system by enabling users to create directories, delete existing directories except for the current working directory, display items in the working directory, and change the current working directory.
 The addDir () function is used to add directories, where a flag is used to determine if it is the first directory in the system's database. In the case that it is not the first directory, it will determine if the inputted directory has similar names, the correct path, free space, and so on. When all criteria are met, a new directory is created from DirItem to find and read its inode address, then update its last inode address, which is then passed to be updated to the superblock, inode bitmap, and block bitmap.
 
 Using the ls () function, the user can output the list of files and directories from the current working directory in the file system along with their file type, size, and date/time created. The system will check the inode address of the current directory and seek the items that have been saved in the database from that address. When the item name is a directory, it will check for its subfiles and then update the directory size accordingly.
 
 The cd () function allows the user to change directories. Upon first boot, the system will begin from the root directory "/". Invocation of this function searches for the inode address of the inputted path and sets the current directory inode address to the new one. It then updates the pwd () function, which serves as a pointer to the working directory, which is called from the main function. 
 
 
 
  	File Functions
The program allows users to create files, delete existing files, copy files to another location, and read the contents of files by printing the file on the terminal.
 To add files to the current directory, we implemented the bool addFile () function that first checks whether the user input path is absolute or relative. Further, the program will check whether the file name and size entered meet all the required specifications such as name, size, and free space. On success, the last inode address will be updated, and a new inode address will be created in the current working directory.

The user can read the content of an existing file, using the cat () function. This function seeks the current inode address of the file and using the 26 alphabets array, generates a random string output based on the file size using srand (). If the inode address for the file cannot be found, the program prints an error message on the terminal. 


Next, void cp () allows the files to be copied to another location in the system. It takes two user inputs, one for source and one for destination. It then checks both inode addresses to see if they are valid. If the destination file is valid, then that file will be removed using the rmFile () function and then a new fille will be created under the same name of the destination file and the filesize of the source file will be copied to the destination file.


 	Current directory functions
The string pwd () function is implemented to detect the current directory users are using and return the path when the system is run/executed, allowing the user to better navigate the system. The ispwd () function checks if the pointer is pointing to the current working directory

 	Remove Functions
This section deals with removing the existing file and directories using the rmFile () and rmDir () function.In the rmFile () function, we check the current inode address of a path by getting its last directory to determine its current directory address. Once the file is successfully deleted the free block number is updated.  
In the rmDir () function, the user cannot delete the current working directory. First, it checks whether the user input path is absolute or relative, then it checks its validity. If it is valid, the system will look up the inode address of that directory and update its last inode, which will then delete the directory from the system and free a block number. Then, the superblock, inodebmp, and blockbmp are updated with the new addresses 



 	MAIN FUNCTION
This function provides the user with an interface through which they can interact with the program. Upon execution, the superblock is loaded first, then the filesystem. A welcome message is displayed followed by a copyright mark with the name and student ID of the group member and a /help command prompt to introduce the available commands.
 When the user enters a command into the program, the corresponding function is invoked to produce the output During program execution, the while(true) loop loops the codes continuously, allowing users to continue using the UNIX filesystem until the program is terminated by the exit command. This is shown in the following figure that illustrates the program flow. 

![image](https://user-images.githubusercontent.com/110232966/181748983-482ccb64-84a6-4b40-9d65-ccb3cde4d0b8.png)





