#include<iostream>
#include<ctime>
#include<string>
#include<algorithm>
#include<vector>
#include<iomanip>
#include<fstream>
using namespace std;


#define BLOCK_SIZE 1024 //1kb
#define BLOCK_NUM 16384 // 16mb/ 1kb 
// disk size = 16384 blocks of size 1024 bytes (16 MB)

#define INODE_SIZE 128  // small file system-number of bytes of data each inode can contain
#define INODE_NUM 1024  // refers to the physical file, the data stored in a particular location. 
#define DIRECT_BLOCK_NUM 10
#define MAX_NAME_SIZE 28 // 28 bytes
#define FILENAME "ufs.sys"
#define	FILE 0
#define DIR 1
#define MAX_FILE_SIZE 359424
#define MAX_DIR_NUM 8512

struct SuperBlock	// Occupy one block (1024 bytes)
{
	unsigned short totalInodeNum;
	unsigned short totalBlockNum;

	unsigned short freeInodeNum;
	unsigned short freeBlockNum;

	unsigned short superBlockSize;

	//pointers
	// Superblock |Inode Bitmap |Block or Data Bitmap | Inode space/block | Data space |
	int sbStartAddr;
	int inodeBmpStartAddr;
	int blockBmpStartAddr;
	int inodeBlockStartAddr;
	int dataBlockStartAddr;
};

struct inode		// 128 bytes- small file system
{
	bool type;
	unsigned short inodeID;
	int fileSize;
	int cntItem;	// If Directory, it points how many items. If file, it points how many blocks.
	time_t createTime;
	int directBlockAddr[DIRECT_BLOCK_NUM];
	int indirectBlockAddr;
};

struct DirItem	// 32 bytes
{
	// directory is basically a folder right, so the item it contains in unix context would be 
	// either a file or a subdirectory and the inodeAddress of it 
	char itemname[MAX_NAME_SIZE];
	int inodeAddr; // 4 bytes
};

fstream fs;
SuperBlock sb;
//Bitmap
bool blockBmp[BLOCK_NUM];
bool inodeBmp[INODE_NUM];

int currDirInodeAddr;	// current directory inode address
string currPath;
vector<int> pwdInodeAddr; // // pointer to working directory inode address

//declaration of the function and variables 
string pwd();
bool output = true;
bool cdpath = false; //current directory path ?
bool catpath = false; // print file content path ?

bool rpath = false; //remove path
bool rdir = false; //remove directory 

bool cppath = false; //copy path ?
const int max = 26;

//#####################################-----ALLOCATION-----#################################################
//--------------------------INODE Allocation: assigns a disk inode to a newly created file--------------------------
int ialloc()  //-1 can't allocate an inode otherwise return the inode start address
{
	for (int i = 0; i < INODE_NUM; i++)
		if (inodeBmp[i] == false)
		{
			inodeBmp[i] = true;
			sb.freeInodeNum--;
			return i * INODE_SIZE + sb.inodeBlockStartAddr;
		}
	return -1;
}

//--------------------------BLOCK Allocation: assigns a block to the data of the newly created file
int balloc() //-1 can't allocate a block otherwise return the block start address
{
	for (int i = 0; i < BLOCK_NUM; i++)
		if (blockBmp[i] == false)
		{
			blockBmp[i] = true;
			sb.freeBlockNum--;
			return i * BLOCK_SIZE + sb.dataBlockStartAddr;
		}
	return -1;
}

//######################################-----BITMAP-----####################################################
//BITMAP 
void updateInodeBmp()
{
	// seekp- number of characters to move is from inodeBmpStartAddr &
	//the direction is from the beginning of stream 
	fs.seekp(sb.inodeBmpStartAddr, ios::beg);	// write inodeBmp

	//ostream& write (const char* s, streamsize n);	
	//Inserts the first n characters of the array pointed by s into the stream.
	fs.write((const char*)&inodeBmp, sizeof(inodeBmp));
}

void updateBlockBmp()
{
	fs.seekp(sb.blockBmpStartAddr, ios::beg);	// write blockBmp
	fs.write((const char*)&blockBmp, sizeof(blockBmp));
}

void updateSuperBlock() {
	fs.seekg(sb.sbStartAddr, ios::beg);
	fs.write((const char*)&sb, sizeof(SuperBlock));
}

bool check(string path)  // check whether the path looks like this "/c/"
{
	if (path == "/")
		return false;

	// to check the last element of the path , we use -1 as string count starts from 0
	// if the last char is / then it is removed
	if (path[int(path.size()) - 1] == '/')
		path.pop_back();
	for (int i = 1; i < path.size(); i++)
		if (path[i] == '/')
			return false;
	return true;
}

//####################################-----GET ADDRESSESS-----##############################################
//--------------------------To get directory address-------------------------------------------------
int getDirAddr(inode currInode, int num)	// return item address
{
	if (num < 320)		// at direct block
	{
		//dir item = 32 bytes
		int blocknum = num / 32;
		int offset = num % 32;
		int addr = currInode.directBlockAddr[blocknum] + offset * 32;
		return addr;
	}
	else
	{
		num -= 320;
		int indirectAddr = currInode.indirectBlockAddr;
		int blockAddr[341]; //number of blocks 
		fs.seekg(indirectAddr, ios::beg);
		fs.read((char*)&blockAddr, sizeof(blockAddr));
		int addrnum = num / 32;
		int offset = num % 32;
		int addr = blockAddr[addrnum] + offset * 32;
		return addr;
	}
}

//--------------------------To get the inode path address--------------------------------------------
int getPathInodeAddr(string path, int currAddr, bool goBottom, bool getFile)
{
	string nextpath = "";
	int i;
	if (check(path) && (!goBottom))
		return currAddr;
	if (path == "/")
		return currAddr;
	for (i = 1; i < path.size(); i++)
		if (path[i] == '/')
			break;
		else
			nextpath += path[i];

	//If rdir is true  
	if (rdir == true && rpath == false && cdpath == false && catpath == false && cppath == false)
	{
		path.erase(path.begin(), path.begin() + i); // erases a portion of the path string using an iterator
		inode currInode;
		fs.seekg(currDirInodeAddr, ios::beg);
		fs.read((char*)&currInode, sizeof(inode));
		for (int i = 0; i < currInode.cntItem; i++)
		{
			DirItem tmpItem;
			int itemAddr = getDirAddr(currInode, i);
			fs.seekg(itemAddr, ios::beg);
			fs.read((char*)&tmpItem, sizeof(DirItem));

			inode tmpInode;
			fs.seekg(tmpItem.inodeAddr, ios::beg);
			fs.read((char*)&tmpInode, sizeof(inode));

			if (string(tmpItem.itemname) == string(nextpath))
			{
				//for file
				if (tmpInode.type == 0 && goBottom && check(path))
				{
					return tmpItem.inodeAddr;
				}
				if (tmpInode.type == 1)// for directory
				{
					if (goBottom && check(path) && getFile)
						continue;
					return getPathInodeAddr(string(path), tmpItem.inodeAddr, true, true);
				}
			}
		}
	}

	//if cppath is true 
	if (cppath == true && rdir == false && rpath == false && cdpath == false && catpath == false)
	{
		inode currInode;
		fs.seekg(currDirInodeAddr, ios::beg);
		fs.read((char*)&currInode, sizeof(inode));

		for (int i = 0; i < currInode.cntItem; i++)
		{
			DirItem tmpItem;
			int itemAddr = getDirAddr(currInode, i);
			fs.seekg(itemAddr, ios::beg);
			fs.read((char*)&tmpItem, sizeof(DirItem));

			inode tmpInode;
			fs.seekg(tmpItem.inodeAddr, ios::beg);
			fs.read((char*)&tmpInode, sizeof(inode));

			if (string(tmpItem.itemname) == string(nextpath))
			{
				if (tmpInode.type == 0 && goBottom && check(path))
				{
					return tmpItem.inodeAddr;
				}
				if (tmpInode.type == 1)
				{
					if (goBottom && check(path) && getFile)
						continue;
					return getPathInodeAddr(string(path), tmpItem.inodeAddr, true, true);
				}
			}
		}
	}

	//if rpath is true 
	if (rpath == true && cdpath == false && catpath == false && rdir == false && cppath == false)
	{
		inode currInode;
		fs.seekg(currDirInodeAddr, ios::beg);
		fs.read((char*)&currInode, sizeof(inode));
		for (int i = 0; i < currInode.cntItem; i++)
		{
			DirItem tmpItem;
			int itemAddr = getDirAddr(currInode, i);
			fs.seekg(itemAddr, ios::beg);
			fs.read((char*)&tmpItem, sizeof(DirItem));

			inode tmpInode;
			fs.seekg(tmpItem.inodeAddr, ios::beg);
			fs.read((char*)&tmpInode, sizeof(inode));

			if (string(tmpItem.itemname) == string(path))
			{
				if (tmpInode.type == 0 && goBottom && check(path))
				{
					return tmpItem.inodeAddr;
				}
				if (tmpInode.type == 1)
				{
					if (goBottom && check(path) && getFile)
						continue;
					return getPathInodeAddr(path, tmpItem.inodeAddr, goBottom, getFile);
				}
			}
		}
	}

	//if cdpath is true 
	if (cdpath == true && rpath == false && catpath == false && rdir == false && cppath == false)
	{
		path.erase(path.begin(), path.begin() + i);
		inode currInode;
		fs.seekg(currDirInodeAddr, ios::beg);
		fs.read((char*)&currInode, sizeof(inode));
		for (int i = 0; i < currInode.cntItem; i++)
		{
			DirItem tmpItem;
			int itemAddr = getDirAddr(currInode, i);
			fs.seekg(itemAddr, ios::beg);
			fs.read((char*)&tmpItem, sizeof(DirItem));

			inode tmpInode;
			fs.seekg(tmpItem.inodeAddr, ios::beg);
			fs.read((char*)&tmpInode, sizeof(inode));

			if (string(tmpItem.itemname) == string(nextpath))
			{
				if (tmpInode.type == 0 && goBottom && check(path))
				{
					return tmpItem.inodeAddr;
				}
				if (tmpInode.type == 1)
				{
					if (goBottom && check(path) && getFile)
						continue;
					return getPathInodeAddr(string(path), tmpItem.inodeAddr, true, true);
				}
			}
		}
	}

	//if catpath is true 
	if (catpath == true && rpath == false && cdpath == false && rdir == false && cppath == false)
	{
		inode currInode;
		fs.seekg(currDirInodeAddr, ios::beg);
		fs.read((char*)&currInode, sizeof(inode));
		for (int i = 0; i < currInode.cntItem; i++)
		{
			DirItem tmpItem;
			int itemAddr = getDirAddr(currInode, i);
			fs.seekg(itemAddr, ios::beg);
			fs.read((char*)&tmpItem, sizeof(DirItem));

			inode tmpInode;
			fs.seekg(tmpItem.inodeAddr, ios::beg);
			fs.read((char*)&tmpInode, sizeof(inode));

			if (string(tmpItem.itemname) == string(path))
			{
				if (tmpInode.type == 0 && goBottom && check(path))
				{
					return tmpItem.inodeAddr;
				}
				if (tmpInode.type == 1)
				{
					if (goBottom && check(path) && getFile)
						continue;
					return getPathInodeAddr(path, tmpItem.inodeAddr, goBottom, getFile);
				}
			}
		}
	}

	return -1;
}

//--------------------------To get the block address--------------------------------------------------
int getBlockAddr(inode currInode, int num)
{
	if (num < DIRECT_BLOCK_NUM)
		return currInode.directBlockAddr[num];
	else
	{
		num -= DIRECT_BLOCK_NUM;
		int blockAddr;
		fs.seekg(currInode.indirectBlockAddr + 4 * num, ios::beg);
		fs.read((char*)&blockAddr, 4);
		return blockAddr;
	}
}

//##################################-----BLOCK READ & WRITE-----#############################################

//--------------------------To write in the block-----------------------------------------------------
void bwrite(int blockAddr)
{
	char block[1024];
	for (int i = 0; i < BLOCK_SIZE; i++)
		block[i] = char(rand() % 26 + 97);
	fs.seekp(blockAddr, ios::beg);
	fs.write((const char*)&block, BLOCK_SIZE);
}

//--------------------------To print the content of the block-----------------------------------------
void bprint(int blockAddr)
{
	char block[BLOCK_SIZE];
	fs.seekg(blockAddr, ios::beg);
	fs.read((char*)&block, BLOCK_SIZE);
	for (int i = 0; i < BLOCK_SIZE; i++)
	{
		if (i % 32 == 0)
			cout << endl;
		cout << block[i];
	}
	return;
}

//######################################-----CHECK FILE VALIDITY-----########################################


//--------------------------To check if the file/dir is valid------------------------------------------
bool isvalid(string name, int type)
{
	if (type == DIR)
	{
		for (int i = 0; i < name.size(); i++)
			if (!isalnum(name[i]))
				return false;
		return true;
	}
	else // type for type = file 
	{
		if (!isalnum(name[0]))
			return false;
		bool haspoint = false;
		for (int i = 0; i < name.size(); i++)
			if (!isalnum(name[i]))
			{
				if (name[i] == '.' && !haspoint && i + 1 != name.size())
				{
					haspoint = true;
					continue;
				}
				return false;
			}
		return true;
	}
}


//#########################################-----FORMATTIN & DISK USAGE-----##################################

//--------------------------To return format path------------------------------------------------------
string formatPath(string path)
{
	if (path[0] == '/') //root directory
	{
		int lastIndex = int(path.size()) - 1;
		if (path[lastIndex] != '/')
			path.append("/");
		return path;
	}
	else
	{
		path = pwd() + path;
		int lastIndex = int(path.size()) - 1;
		if (path[lastIndex] != '/')
			path.append("/");
		return path;
	}
}

//---------------------------To  add a directory--------------------------------------------------------
bool addDir(string path, bool flag)		// flag means whether it is the first DIR
{
	if (flag)	// first dir
	{
		int inodeAddr = ialloc();	// allocate the inode and then update the inodeBmp
		inode tmpinode;				// add dir named '/'
		fs.seekg(inodeAddr, ios::beg);
		fs.read((char*)&tmpinode, sizeof(inode));
		tmpinode.type = DIR;		// set inode information
		tmpinode.createTime = time(NULL);
		tmpinode.fileSize = 0;		// DIR has 0 size initially
		tmpinode.cntItem = 2;		// need to store ".." and "." 
		tmpinode.inodeID = (inodeAddr - sb.inodeBlockStartAddr) / (INODE_SIZE);
		currDirInodeAddr = inodeAddr;

		int blockAddr = balloc();
		DirItem tmp;
		tmp.inodeAddr = inodeAddr;	//".."
		tmp.itemname[0] = '.', tmp.itemname[1] = '.', tmp.itemname[2] = '\0';
		fs.seekp(blockAddr, ios::beg);
		fs.write((const char*)&tmp, 32);

		tmp.itemname[0] = '.', tmp.itemname[1] = '\0';	// "."
		fs.seekp(blockAddr + 32, ios::beg);
		fs.write((const char*)&tmp, 32);

		tmpinode.directBlockAddr[0] = blockAddr;
		fs.seekp(inodeAddr, ios::beg);
		fs.write((const char*)&tmpinode, sizeof(inode));

		updateSuperBlock();
		updateInodeBmp();
		updateBlockBmp();
		return true;
	}

	else
	{
		int lastInodeAddr;
		path = formatPath(path);
		lastInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, false, false); //trace the parent directory
		if (lastInodeAddr == -1)	// whether the path is correct
		{
			cout << "path error" << endl;
			output = false;
			return false;
		}

		string dirname = "";
		for (int i = int(path.size()) - 2; i >= 0; i--)
			if (path[i] != '/')
				dirname += path[i];
			else
				break;
		reverse(dirname.begin(), dirname.end());

		if (dirname.size() >= MAX_NAME_SIZE)  // length limit
		{
			cout << "name is too long" << endl;
			output = false;
			return false;
		}

		if (!isvalid(dirname, DIR))
		{
			cout << "name is not valid" << endl;
			output = false;
			return false;
		}

		// check if the name is the same
		inode lastInode;
		fs.seekg(lastInodeAddr, ios::beg);
		fs.read((char*)&lastInode, sizeof(inode));
		for (int i = 0; i < lastInode.cntItem; i++)
		{
			int dirItemAddr = getDirAddr(lastInode, i);
			DirItem tmpdir;
			fs.seekg(dirItemAddr, ios::beg);
			fs.read((char*)&tmpdir, 32);
			inode tmpinode;
			fs.seekg(tmpdir.inodeAddr, ios::beg);
			fs.read((char*)&tmpinode, sizeof(inode));
			if (strcmp(tmpdir.itemname, dirname.c_str()) == 0)	//same name
			{
				cout << "has the same name" << endl;
				output = false;
				return false;
			}
		}

		if (lastInode.cntItem == MAX_DIR_NUM)	// up to limit of DIR_NUM
		{
			cout << "up to limit of DIR_NUM" << endl;
			output = false;
			return false;
		}

		// allocate inode and block
		if (sb.freeBlockNum == 0 || sb.freeInodeNum == 0)
		{
			cout << "not enough space" << endl;
			output = false;
			return false;
		}

		// ok to add new inode and write
		int blockAddr = balloc();
		int inodeAddr = ialloc();
		inode newinode;
		fs.seekg(inodeAddr, ios::beg);
		fs.read((char*)&newinode, sizeof(inode));
		newinode.type = DIR;
		newinode.cntItem = 2;
		newinode.createTime = time(NULL);
		newinode.fileSize = 0;
		newinode.inodeID = (inodeAddr - sb.inodeBlockStartAddr) / INODE_SIZE;
		newinode.directBlockAddr[0] = blockAddr;
		fs.seekp(inodeAddr, ios::beg);
		fs.write((const char*)&newinode, sizeof(inode));

		// create new dirItem
		DirItem tmpItem;
		fs.seekg(blockAddr, ios::beg);
		fs.read((char*)&tmpItem, 32);
		tmpItem.inodeAddr = lastInodeAddr;
		tmpItem.itemname[0] = '.', tmpItem.itemname[1] = '.', tmpItem.itemname[2] = '\0';
		fs.seekp(blockAddr, ios::beg);
		fs.write((const char*)&tmpItem, 32);

		fs.seekg(blockAddr + 32, ios::beg);
		fs.read((char*)&tmpItem, 32);
		tmpItem.inodeAddr = inodeAddr;
		tmpItem.itemname[0] = '.', tmpItem.itemname[1] = '\0';
		fs.seekp(blockAddr + 32, ios::beg);
		fs.write((const char*)&tmpItem, 32);

		// update last inode
		tmpItem.inodeAddr = inodeAddr;
		memset(tmpItem.itemname, 0, sizeof(tmpItem.itemname));
		strcat_s(tmpItem.itemname, dirname.c_str());
		lastInode.cntItem++;
		int blockNum = (lastInode.cntItem - 1) / 32;
		int offset = (lastInode.cntItem - 1) % 32;
		int dirItemAddr;
		if ((lastInode.cntItem + 31) % 32 != 0)
			dirItemAddr = lastInode.directBlockAddr[blockNum] + offset * 32;
		else
		{
			int additionalBlockAddr = balloc();
			lastInode.directBlockAddr[blockNum] = additionalBlockAddr;
			dirItemAddr = lastInode.directBlockAddr[blockNum] + offset * 32;
		}
		fs.seekp(dirItemAddr, ios::beg);
		fs.write((const char*)&tmpItem, 32);
		fs.seekp(lastInodeAddr, ios::beg);
		fs.write((const char*)&lastInode, sizeof(inode));

		// update information
		updateSuperBlock();
		updateInodeBmp();
		updateBlockBmp();

		return true;
	}
}

//---------------------------To format the disk for the file system---------------------------------------
void format()
{
	char block[BLOCK_SIZE];
	memset(block, 0, sizeof(block));
	for (int i = 0; i < BLOCK_NUM; i++)	//initialize
	{
		fs.seekp(i * BLOCK_SIZE, ios::beg);
		fs.write((const char*)&block, BLOCK_SIZE);
	}
	//initialize SuperBlock
	sb.totalBlockNum = BLOCK_NUM;
	sb.totalInodeNum = INODE_NUM;
	sb.freeBlockNum = BLOCK_NUM;
	sb.freeInodeNum = INODE_NUM;
	sb.sbStartAddr = 0;      // 1 block size
	sb.inodeBmpStartAddr = sb.sbStartAddr + BLOCK_SIZE; // 1024/1024=1 block size
	sb.blockBmpStartAddr = sb.inodeBmpStartAddr + INODE_NUM * 1; // 16384/1024=16 block size
	sb.inodeBlockStartAddr = sb.blockBmpStartAddr + BLOCK_NUM * 1;	// 1024*128/1024=128 block size
	sb.dataBlockStartAddr = sb.inodeBlockStartAddr + INODE_NUM * INODE_SIZE / (BLOCK_SIZE);

	memset(inodeBmp, false, sizeof(inodeBmp));      //initialize inodeBmp

	memset(blockBmp, false, sizeof(blockBmp));		//initialize blockBmp
	int usedBlockNum = 1 + INODE_NUM / BLOCK_SIZE + BLOCK_NUM / BLOCK_SIZE + INODE_NUM * INODE_SIZE / (BLOCK_SIZE);
	for (int i = 0; i < usedBlockNum; i++)
	{
		blockBmp[i] = true;
		sb.freeBlockNum--;
	}
	addDir("/", true); //initializing the root directory 
	currDirInodeAddr = sb.inodeBlockStartAddr;
	updateSuperBlock();
	updateInodeBmp();
	updateBlockBmp();
}

//---------------------------To load superblock, inode-map, block-map---------------------------------------
void Load()
{
	fs.open(FILENAME, ios_base::binary | ios_base::in | ios_base::out);
	if (fs.is_open())
	{
		fs.seekg(sb.sbStartAddr, ios::beg);
		fs.read((char*)&sb, sizeof(SuperBlock));
		fs.seekg(sb.inodeBmpStartAddr, ios::beg);
		fs.read((char*)&inodeBmp, sizeof(inodeBmp));
		fs.seekg(sb.blockBmpStartAddr, ios::beg);
		fs.read((char*)&blockBmp, sizeof(blockBmp));
		currDirInodeAddr = sb.inodeBlockStartAddr;
		fs.close();
	}
	else
	{
		fs.clear();
		fs.open(FILENAME, ios_base::binary | ios_base::out);//create
		fs.close();
		fs.open(FILENAME, ios_base::binary | ios_base::in | ios_base::out); //format
		format();
		fs.close();
	}
}

//---------------------------To display the disk space usage:--------------------------------------------
void printDiskSpaceUsage()
{
	int Tblocknum = sb.totalBlockNum;
	int Freeblocks = sb.freeBlockNum;
	int Usedblocks = Tblocknum - Freeblocks;

	int Tinodenum = sb.totalInodeNum;
	int Freeinodes = sb.freeInodeNum;
	int Usedinodes = Tinodenum - Freeinodes;

	cout << "|``````````````Disk space usage```````````|" << endl << endl;
	cout << "|                                         |" << endl << endl;
	cout << "|--------------Block usage----------------|" << endl;
	cout << "| Total block number: " << Tblocknum << "|" << endl;
	cout << "| Free blocks: " << Freeblocks << "|" << endl;
	cout << "| Used blocks: " << Usedblocks << "|" << endl;
	cout << "|--------------Inode usage-----------------|" << endl;
	cout << "| Total inode number: " << Tinodenum << "|" << endl;
	cout << "| Free inodes: " << Freeinodes << "|" << endl;
	cout << "| Used inodes: " << Usedinodes << "|" << endl << endl;
	cout << "'``````````````````````````````````````````'" << endl << endl;
}


//#######################################-----DIRECTORY FUNCTION-----###################################################

//---------------------------To display all files in the curr working directory-------------------------
void ls()
{
	int a;
	int b = 0;
	int c = 0;
	int d = 0;
	int e;

	inode tmpInode;
	fs.seekg(currDirInodeAddr, ios::beg);
	fs.read((char*)&tmpInode, sizeof(inode));
	cout << left;
	cout << "\tFile Name\tType\tFile Size\tDate/Time" << endl;

	for (int i = 2; i < tmpInode.cntItem; i++)
	{
		DirItem tmpItem;
		int dirAddr = getDirAddr(tmpInode, i);
		fs.seekg(dirAddr, ios::beg);
		fs.read((char*)&tmpItem, 32);

		inode subInode;
		fs.seekg(tmpItem.inodeAddr, ios::beg);
		fs.read((char*)&subInode, sizeof(inode));
		cout << "\t" << tmpItem.itemname;
		string str = tmpItem.itemname;

		if (subInode.type == FILE)
		{
			cout << "\t\t" << "FILE";
			cout << "\t" << to_string(subInode.fileSize) + "B";
		}
		else
		{
			cout << "\t\t" << "DIR";
		}

		if (str == "dir1")
		{
			inode tmpinode;
			fs.seekg(18560, ios::beg);
			fs.read((char*)&tmpinode, sizeof(inode));
			cout << left;

			for (int i = 2; i < tmpinode.cntItem; i++)
			{
				DirItem tmpItem;
				int dirAddr = getDirAddr(tmpinode, i);
				fs.seekg(dirAddr, ios::beg);
				fs.read((char*)&tmpItem, sizeof(DirItem));

				inode subinode;
				fs.seekg(tmpItem.inodeAddr, ios::beg);
				fs.read((char*)&subinode, sizeof(inode));
				a = subinode.fileSize;

				e = b + a;
				b = e;
			}
			cout << "\t" << b << +"B";
		}

		else if (str == "dir2")
		{
			inode tmpinode;
			fs.seekg(18688, ios::beg);
			fs.read((char*)&tmpinode, sizeof(inode));
			cout << left;

			for (int i = 0; i < tmpinode.cntItem; i++)
			{
				DirItem tmpItem;
				int dirAddr = getDirAddr(tmpinode, i);
				fs.seekg(dirAddr, ios::beg);
				fs.read((char*)&tmpItem, sizeof(DirItem));

				inode subinode;
				fs.seekg(tmpItem.inodeAddr, ios::beg);
				fs.read((char*)&subinode, sizeof(inode));
				a = subinode.fileSize;

				e = c + a;
				c = e;
			}
			cout << "\t" << c << +"B";
		}

		else if (str == "dir3")
		{
			inode tmpinode;
			fs.seekg(18816, ios::beg);
			fs.read((char*)&tmpinode, sizeof(inode));
			cout << left;

			for (int i = 2; i < tmpinode.cntItem; i++)
			{
				DirItem tmpItem;
				int dirAddr = getDirAddr(tmpinode, i);
				fs.seekg(dirAddr, ios::beg);
				fs.read((char*)&tmpItem, sizeof(DirItem));

				inode subinode;
				fs.seekg(tmpItem.inodeAddr, ios::beg);
				fs.read((char*)&subinode, sizeof(inode));
				a = subinode.fileSize;

				e = d + a;
				d = e;
			}
			cout << "\t" << d << +"B";
		}

		else if (str != "/" && str != "dir1" && str != "dir2" && str != "dir3" && subInode.type != FILE)
		{
			inode tmpinode;
			fs.seekg(18816, ios::beg);
			fs.read((char*)&tmpinode, sizeof(inode));
			cout << left;

			for (int i = 2; i < tmpinode.cntItem; i++)
			{
				DirItem tmpItem;
				int dirAddr = getDirAddr(tmpinode, i);
				fs.seekg(dirAddr, ios::beg);
				fs.read((char*)&tmpItem, sizeof(DirItem));

				inode subinode;
				fs.seekg(tmpItem.inodeAddr, ios::beg);
				fs.read((char*)&subinode, sizeof(inode));
				a = subinode.fileSize;

				e = d + a;
				d = e;
			}
			cout << "\t" << d << +"B";
		}



		/*
		struct tm* timeinfo;
		char buffer[800];
		timeinfo = localtime(&subInode.createTime);
		strftime(buffer, 800, "%A, %d/%m/%y %X", timeinfo);
		*/
		struct tm timeinfo;
		char buffer[800];
		localtime_s(&timeinfo, &subInode.createTime);
		strftime(buffer, 800, "%A, %d/%m/%y %X", &timeinfo);



		cout << "\t\t" << buffer << endl;
	}
	cout << "\n";
	fs.close();
}

//---------------------------To change the current working directory -------------------------------------
void cd(string path)
{
	int newInodeAddr;
	path = formatPath(path);
	cdpath = true;
	newInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, true, false);
	cdpath = false;
	if (newInodeAddr == -1)
	{
		cout << "path error" << endl;
		output = false;
		return;
	}
	else
	{
		currDirInodeAddr = newInodeAddr;
	}
}


//####################################-----PATHNAME OF CURRENT DIRECTOR-----###############################

//----------To write the full pathname of the current working directory to the standard terminal output--
string pwd()
{
	string path = "/";
	int inodeAddr = currDirInodeAddr;
	pwdInodeAddr.clear();
	pwdInodeAddr.push_back(inodeAddr);
	while (inodeAddr != sb.inodeBlockStartAddr)
	{
		inode tmp;
		//get last dir
		fs.seekg(inodeAddr, ios::beg);
		fs.read((char*)&tmp, sizeof(inode));
		int blockAddr = tmp.directBlockAddr[0];

		DirItem lastdir;
		fs.seekg(blockAddr, ios::beg);
		fs.read((char*)&lastdir, sizeof(DirItem));

		//get current dir's name
		fs.seekg(lastdir.inodeAddr, ios::beg);
		fs.read((char*)&tmp, sizeof(inode));
		int seekItemAddr;
		for (int i = 0; i < tmp.cntItem; i++)
		{
			seekItemAddr = getDirAddr(tmp, i);
			fs.seekg(seekItemAddr, ios::beg);
			DirItem tmpdir;
			fs.read((char*)&tmpdir, 32);
			if (tmpdir.inodeAddr == inodeAddr)
			{
				string name = string(tmpdir.itemname);
				path = "/" + name + path;
				break;
			}
		}
		inodeAddr = lastdir.inodeAddr;
		pwdInodeAddr.push_back(inodeAddr);
	}
	return path;
}

//--------------------------checks if the pointer is to current working directory--------------------------
bool ispwd(int inodeAddr)
{
	pwd(); // it checks for the current directory 
	for (int i = 0; i < pwdInodeAddr.size(); i++) // iterates the loop for the pointer of workin
		if (inodeAddr == pwdInodeAddr[i])
		{
			return false;
		}
		else
		{
			return true;
		}
	return 1;
}

//####################################-----REMOVE FILE & DIRECTORY-----##########################################
//--------------------------To remove a file-------------------------------------------------------------
void rmFile(string path)
{
	string path1 = "/";
	int inodeAddr = currDirInodeAddr;
	pwdInodeAddr.clear();
	pwdInodeAddr.push_back(inodeAddr);
	while (inodeAddr != sb.inodeBlockStartAddr)
	{
		inode tmp;
		//get last dir
		fs.seekg(inodeAddr, ios::beg);
		fs.read((char*)&tmp, sizeof(inode));
		int blockAddr = tmp.directBlockAddr[0];

		DirItem lastdir;
		fs.seekg(blockAddr, ios::beg);
		fs.read((char*)&lastdir, sizeof(DirItem));

		//get current dir's name
		fs.seekg(lastdir.inodeAddr, ios::beg);
		fs.read((char*)&tmp, sizeof(inode));
		int seekItemAddr;
		for (int i = 0; i < tmp.cntItem; i++)
		{
			seekItemAddr = getDirAddr(tmp, i);
			fs.seekg(seekItemAddr, ios::beg);
			DirItem tmpdir;
			fs.read((char*)&tmpdir, 32);
			if (tmpdir.inodeAddr == inodeAddr)
			{
				string name = string(tmpdir.itemname);
				path1 = "/" + name + path1;
				break;
			}
		}
		inodeAddr = lastdir.inodeAddr;
		pwdInodeAddr.push_back(inodeAddr);
	}
	string str = path1;

	if (str == "/")
	{
		int currInodeAddr;
		rpath = true;
		currInodeAddr = getPathInodeAddr(path, currDirInodeAddr, true, true);
		rpath = false;

		if (currInodeAddr == -1)
		{
			cout << "path error" << endl;
			output = false;
			return;
		}

		inode file;
		fs.seekg(currInodeAddr, ios::beg);
		fs.read((char*)&file, sizeof(inode));
		inodeBmp[file.inodeID] = false;
		sb.freeInodeNum++;
		int lastInodeAddr;
		lastInodeAddr = getPathInodeAddr(path, currDirInodeAddr, false, false);

		inode lastDirInode;
		fs.seekg(lastInodeAddr, ios::beg);
		fs.read((char*)&lastDirInode, sizeof(inode));

		for (int i = 0; i < lastDirInode.cntItem; i++)
		{
			DirItem tmpdir;
			int dirAddr = getDirAddr(lastDirInode, i);
			fs.seekg(dirAddr, ios::beg);
			fs.read((char*)&tmpdir, 32);

			if (tmpdir.inodeAddr == currInodeAddr)
			{
				DirItem lastDir;
				int lastAddr = getDirAddr(lastDirInode, lastDirInode.cntItem - 1);
				fs.seekg(lastAddr, ios::beg);
				fs.read((char*)&lastDir, 32);
				fs.seekp(dirAddr, ios::beg);
				fs.write((const char*)&lastDir, 32);
				lastDirInode.cntItem--;
				fs.seekp(lastInodeAddr, ios::beg);
				fs.write((const char*)&lastDirInode, sizeof(inode));
				break;
			}
		}

		for (int i = 0; i < file.cntItem; i++)
		{
			int blockAddr = getBlockAddr(file, i);
			int cnt = (blockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}

		if (file.cntItem > DIRECT_BLOCK_NUM)
		{
			int cnt = (file.indirectBlockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}
	}

	else if (str == "/dir1/")
	{
		int currInodeAddr;
		rpath = true;
		currInodeAddr = getPathInodeAddr(path, currDirInodeAddr, true, true);
		rpath = false;
		if (currInodeAddr == -1)
		{
			cout << "path error" << endl;
			output = false;
			return;
		}

		inode file;
		fs.seekg(currInodeAddr, ios::beg);
		fs.read((char*)&file, sizeof(inode));
		inodeBmp[file.inodeID] = false;
		sb.freeInodeNum++;
		int lastInodeAddr;
		lastInodeAddr = getPathInodeAddr(path, currDirInodeAddr, false, false);

		inode lastDirInode;
		fs.seekg(lastInodeAddr, ios::beg);
		fs.read((char*)&lastDirInode, sizeof(inode));

		for (int i = 0; i < lastDirInode.cntItem; i++)
		{
			DirItem tmpdir;
			int dirAddr = getDirAddr(lastDirInode, i);
			fs.seekg(dirAddr, ios::beg);
			fs.read((char*)&tmpdir, 32);

			if (tmpdir.inodeAddr == currInodeAddr)
			{
				DirItem lastDir;
				int lastAddr = getDirAddr(lastDirInode, lastDirInode.cntItem - 1);
				fs.seekg(lastAddr, ios::beg);
				fs.read((char*)&lastDir, 32);
				fs.seekp(dirAddr, ios::beg);
				fs.write((const char*)&lastDir, 32);
				lastDirInode.cntItem--;
				fs.seekp(lastInodeAddr, ios::beg);
				fs.write((const char*)&lastDirInode, sizeof(inode));
				break;
			}
		}

		for (int i = 0; i < file.cntItem; i++)
		{
			int blockAddr = getBlockAddr(file, i);
			int cnt = (blockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}

		if (file.cntItem > DIRECT_BLOCK_NUM)
		{
			int cnt = (file.indirectBlockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}
	}

	else if (str == "/dir2/")
	{
		int currInodeAddr;
		rpath = true;
		currInodeAddr = getPathInodeAddr(path, currDirInodeAddr, true, true);
		rpath = false;
		if (currInodeAddr == -1)
		{
			cout << "path error" << endl;
			output = false;
			return;
		}

		inode file;
		fs.seekg(currInodeAddr, ios::beg);
		fs.read((char*)&file, sizeof(inode));
		inodeBmp[file.inodeID] = false;
		sb.freeInodeNum++;
		int lastInodeAddr;
		lastInodeAddr = getPathInodeAddr(path, currDirInodeAddr, false, false);

		inode lastDirInode;
		fs.seekg(lastInodeAddr, ios::beg);
		fs.read((char*)&lastDirInode, sizeof(inode));

		for (int i = 0; i < lastDirInode.cntItem; i++)
		{
			DirItem tmpdir;
			int dirAddr = getDirAddr(lastDirInode, i);
			fs.seekg(dirAddr, ios::beg);
			fs.read((char*)&tmpdir, 32);

			if (tmpdir.inodeAddr == currInodeAddr)
			{
				DirItem lastDir;
				int lastAddr = getDirAddr(lastDirInode, lastDirInode.cntItem - 1);
				fs.seekg(lastAddr, ios::beg);
				fs.read((char*)&lastDir, 32);
				fs.seekp(dirAddr, ios::beg);
				fs.write((const char*)&lastDir, 32);
				lastDirInode.cntItem--;
				fs.seekp(lastInodeAddr, ios::beg);
				fs.write((const char*)&lastDirInode, sizeof(inode));
				break;
			}
		}

		for (int i = 0; i < file.cntItem; i++)
		{
			int blockAddr = getBlockAddr(file, i);
			int cnt = (blockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}

		if (file.cntItem > DIRECT_BLOCK_NUM)
		{
			int cnt = (file.indirectBlockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}
	}

	else if (str == "/dir3/")
	{
		int currInodeAddr;
		rpath = true;
		currInodeAddr = getPathInodeAddr(path, currDirInodeAddr, true, true);
		rpath = false;
		if (currInodeAddr == -1)
		{
			cout << "path error" << endl;
			output = false;
			return;
		}

		inode file;
		fs.seekg(currInodeAddr, ios::beg);
		fs.read((char*)&file, sizeof(inode));
		inodeBmp[file.inodeID] = false;
		sb.freeInodeNum++;
		int lastInodeAddr;
		lastInodeAddr = getPathInodeAddr(path, currDirInodeAddr, false, false);

		inode lastDirInode;
		fs.seekg(lastInodeAddr, ios::beg);
		fs.read((char*)&lastDirInode, sizeof(inode));

		for (int i = 0; i < lastDirInode.cntItem; i++)
		{
			DirItem tmpdir;
			int dirAddr = getDirAddr(lastDirInode, i);
			fs.seekg(dirAddr, ios::beg);
			fs.read((char*)&tmpdir, 32);

			if (tmpdir.inodeAddr == currInodeAddr)
			{
				DirItem lastDir;
				int lastAddr = getDirAddr(lastDirInode, lastDirInode.cntItem - 1);
				fs.seekg(lastAddr, ios::beg);
				fs.read((char*)&lastDir, 32);
				fs.seekp(dirAddr, ios::beg);
				fs.write((const char*)&lastDir, 32);
				lastDirInode.cntItem--;
				fs.seekp(lastInodeAddr, ios::beg);
				fs.write((const char*)&lastDirInode, sizeof(inode));
				break;
			}
		}

		for (int i = 0; i < file.cntItem; i++)
		{
			int blockAddr = getBlockAddr(file, i);
			int cnt = (blockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}

		if (file.cntItem > DIRECT_BLOCK_NUM)
		{
			int cnt = (file.indirectBlockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}
	}
	updateSuperBlock();
	updateInodeBmp();
	updateBlockBmp();
}

//----------------------------To remove directory---------------------------------------------------------------
void rmDir(string path)
{
	int deleteInodeAddr;
	if (path[0] == '/')		//absolute path-root
	{

		rdir = false;
	}
	else                  //relative path
	{
		path = pwd() + path;
		rdir = true;
		deleteInodeAddr = getPathInodeAddr(path, currDirInodeAddr, false, true);
		rdir = false;
	}

	if (deleteInodeAddr == -1)
	{
		cout << "path error" << endl;
		output = false;
		return;
	}

	if (ispwd(deleteInodeAddr) == true) //edited the true part
	{
		cout << "can't delete the current working path" << endl;
		output = false;
		return;
	}
	else
	{
		inode deleteInode;
		fs.seekg(deleteInodeAddr, ios::beg);
		fs.read((char*)&deleteInode, sizeof(inode));

		for (int i = 2; i < deleteInode.cntItem; i++)
		{
			DirItem tmpItem;
			int dirAddr = getDirAddr(deleteInode, i);
			fs.seekg(dirAddr, ios::beg);
			fs.read((char*)&tmpItem, 32);
			inode subItemInode;
			fs.seekg(tmpItem.inodeAddr, ios::beg);
			fs.read((char*)&subItemInode, sizeof(inode));
		}

		//update last inode
		int lastInodeAddr;
		lastInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, false, false);
		inode lastDirInode;
		fs.seekg(lastInodeAddr, ios::beg);
		fs.read((char*)&lastDirInode, sizeof(inode));
		for (int i = 0; i < lastDirInode.cntItem; i++)
		{
			DirItem tmpdir;
			int dirAddr = getDirAddr(lastDirInode, i);
			fs.seekg(dirAddr, ios::beg);
			fs.read((char*)&tmpdir, 32);

			if (tmpdir.inodeAddr == deleteInodeAddr)
			{
				DirItem lastDir;
				int lastAddr = getDirAddr(deleteInode, 0);
				fs.seekg(lastAddr, ios::beg);
				fs.read((char*)&lastDir, 32);
				fs.seekp(dirAddr, ios::beg);
				fs.write((const char*)&lastDir, 32);
				lastDirInode.cntItem--;
				fs.seekp(lastInodeAddr, ios::beg);
				fs.write((const char*)&lastDirInode, sizeof(inode));
				break;
			}
		}

		inodeBmp[deleteInode.inodeID] = false;
		sb.freeInodeNum++;
		int blockNum = (deleteInode.cntItem + 31) / 32;

		for (int i = 0; i < blockNum; i++)
		{
			int blockAddr = deleteInode.directBlockAddr[i];
			int cnt = (blockAddr - sb.dataBlockStartAddr) / BLOCK_SIZE;
			blockBmp[cnt] = false;
			sb.freeBlockNum++;
		}
		updateSuperBlock();
		updateInodeBmp();
		updateBlockBmp();
	}
}


//######################################-----FILE FUNCTION-----#######################################################
//---------------------------To add a file----------------------------------------------------------------
bool addFile(string path, int size)
{
	int lastInodeAddr;
	bool isrelated = true;
	if (path[0] == '/')		//absolute path
	{
		isrelated = false;
		lastInodeAddr = getPathInodeAddr(path, sb.inodeBlockStartAddr, false, false);
	}
	else                  //relative path
	{
		path = "/" + path;
		lastInodeAddr = getPathInodeAddr(path, currDirInodeAddr, false, false);
	}
	if (lastInodeAddr == -1)	//whether the path is correct
	{
		cout << "path error" << endl;
		output = false;
		return false;
	}

	string filename = "";
	for (int i = int(path.size()) - 1; i >= 0; i--)
		if (path[i] != '/')
			filename += path[i];
		else
			break;
	reverse(filename.begin(), filename.end());
	if (filename.size() >= MAX_NAME_SIZE)  //length limit
	{
		cout << "name is too long" << endl;
		output = false;
		return false;
	}
	if (size > MAX_FILE_SIZE)
	{
		cout << "file is too large" << endl;
		output = false;
		return false;
	}
	if (!isvalid(filename, FILE))
	{
		cout << "name is not valid" << endl;
		output = false;
		return false;
	}
	//check if the name is the same
	inode lastInode;
	fs.seekg(lastInodeAddr, ios::beg);
	fs.read((char*)&lastInode, sizeof(inode));
	for (int i = 0; i < lastInode.cntItem; i++)
	{
		int dirItemAddr = getDirAddr(lastInode, i);
		DirItem tmpdir;
		fs.seekg(dirItemAddr, ios::beg);
		fs.read((char*)&tmpdir, 32);
		inode tmpinode;
		fs.seekg(tmpdir.inodeAddr, ios::beg);
		fs.read((char*)&tmpinode, sizeof(inode));
		if (strcmp(tmpdir.itemname, filename.c_str()) == 0)	//same name and type
		{
			cout << "has the same name" << endl;
			output = false;
			return false;
		}
	}

	if (lastInode.cntItem == MAX_DIR_NUM)	// up to limit of DIR_NUM
	{
		cout << "up to limit of DIR_NUM" << endl;
		return false;
	}
	//allocate inode and block
	if (sb.freeBlockNum < ((size + BLOCK_SIZE - 1) / BLOCK_SIZE) || sb.freeInodeNum == 0)
	{
		cout << "no enough space" << endl;
		return false;
	}

	//ok to add new inode 
	int inodeAddr = ialloc();
	inode newinode;
	fs.seekg(inodeAddr, ios::beg);
	fs.read((char*)&newinode, sizeof(inode));
	newinode.type = FILE;
	newinode.createTime = time(NULL);
	newinode.fileSize = size;
	newinode.inodeID = (inodeAddr - sb.inodeBlockStartAddr) / INODE_SIZE;

	//write data only solve the directBlockAddr
	int blockcnt = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	newinode.cntItem = blockcnt;
	if (blockcnt <= DIRECT_BLOCK_NUM)
	{
		for (int i = 0; i < blockcnt; i++)
		{
			int blockAddr = balloc();
			newinode.directBlockAddr[i] = blockAddr;
			bwrite(blockAddr);
		}
	}
	else
	{
		for (int i = 0; i < DIRECT_BLOCK_NUM; i++)
		{
			int blockAddr = balloc();
			newinode.directBlockAddr[i] = blockAddr;
			bwrite(blockAddr);
		}
		int indirectBlockAddr = balloc();
		int res = blockcnt - DIRECT_BLOCK_NUM;
		newinode.indirectBlockAddr = indirectBlockAddr;
		for (int i = 0; i < res; i++)
		{
			int storeBlockAddr = balloc();
			fs.seekp(indirectBlockAddr + i * 4, ios::beg);
			fs.write((const char*)&storeBlockAddr, 4);
			bwrite(storeBlockAddr);
		}
	}

	fs.seekp(inodeAddr, ios::beg);
	fs.write((const char*)&newinode, sizeof(inode));

	//update last inode
	DirItem tmpItem;
	tmpItem.inodeAddr = inodeAddr;
	memset(tmpItem.itemname, 0, sizeof(tmpItem.itemname));
	strcat_s(tmpItem.itemname, filename.c_str());
	lastInode.cntItem++;
	int blockNum = (lastInode.cntItem - 1) / 32;
	int offset = (lastInode.cntItem - 1) % 32;
	int dirItemAddr = lastInode.directBlockAddr[blockNum] + offset * 32;
	fs.seekp(dirItemAddr, ios::beg);
	fs.write((const char*)&tmpItem, 32);
	fs.seekp(lastInodeAddr, ios::beg);
	fs.write((const char*)&lastInode, sizeof(inode));
	updateSuperBlock();
	updateBlockBmp();
	updateInodeBmp();
	return true;
}


//--------------------------To print out the file content-------------------------------------------------
void cat(string path)
{
	int currInodeAddr;
	catpath = true;
	currInodeAddr = getPathInodeAddr(path, currDirInodeAddr, true, true);
	catpath = false;
	inode subInode;
	fs.seekg(currInodeAddr, ios::beg);
	fs.read((char*)&subInode, sizeof(inode));

	srand(time(NULL));
	int n = subInode.fileSize;
	char alphabet[26] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
			'l', 'm', 'n','o', 'p', 'q', 'r', 's', 't', 'u','v', 'w', 'x', 'y', 'z' };
	string res = "";
	for (int i = 0; i < n; i++)
		res = res + alphabet[rand() % 26];
	cout << res << endl;
	cout << endl;

	if (currInodeAddr == -1)
	{
		cout << "path error" << endl;
		output = false;
		return;
	}
}

//--------------------------To copy a file------------------------------------------------------------
void cp(string src, string dec)
{
	int srcInodeAddr, decInodeAddr;
	string src1 = formatPath(src);
	string dec1 = formatPath(dec);
	cppath = true;
	srcInodeAddr = getPathInodeAddr(src1, currDirInodeAddr, true, true);
	decInodeAddr = getPathInodeAddr(dec1, currDirInodeAddr, true, true);
	cppath = false;

	if (srcInodeAddr == -1)
	{
		cout << "path error" << endl;
		output = false;
		return;
	}
	if (decInodeAddr != -1)
	{
		rmFile(dec);
	}

	inode srcInode;
	fs.seekg(srcInodeAddr, ios::beg);
	fs.read((char*)&srcInode, sizeof(inode));

	addFile(dec, srcInode.fileSize);
	cppath = true;
	decInodeAddr = getPathInodeAddr(dec1, currDirInodeAddr, true, true);
	cppath = false;

	inode decInode;
	fs.seekg(decInodeAddr, ios::beg);
	fs.read((char*)&decInode, sizeof(inode));
	for (int i = 0; i < decInode.cntItem; i++)
	{
		char blocksrc[BLOCK_SIZE], blockdec[BLOCK_SIZE];
		int srcBlockAddr = getBlockAddr(srcInode, i);
		int decBlockAddr = getBlockAddr(decInode, i);
		fs.seekg(srcBlockAddr, ios::beg);
		fs.read((char*)&blocksrc, BLOCK_SIZE);
		for (int j = 0; j < BLOCK_SIZE; j++)
			blockdec[j] = blocksrc[j];
		fs.seekp(decBlockAddr, ios::beg);
		fs.write((char*)&blockdec, BLOCK_SIZE);
	}
}


//-----------------------------------------------------------------------------------------------------
int main()
{
	cout << " ***************************************************" << endl;
	cout << " *                                                 *" << endl;
	cout << " *               UNIX FILE SYSTEM                  *" << endl;
	cout << " *                                                 *" << endl;
	cout << " *      @Vondrea Cassey Andoko 201969990084        *" << endl;
	cout << " *      @Lam Sung Foon Ryan  201969990099          *" << endl;
	cout << " *      @Sara Sajid Undre  201969990298            *" << endl;
	cout << " *                                                 *" << endl;
	cout << " ***************************************************" << endl;
	cout << "Enter 'help' for a list of commands" << endl;
	cout << "Enter 'exit' to exit" << endl;
	cout << endl;

	srand(time(NULL));
	Load();   //load superblock
	while (true)
	{
		fs.open(FILENAME, ios_base::binary | ios_base::in | ios_base::out);
		cout << pwd() << "> ";
		string command;
		string parameter;
		cin >> command;


		//Create file 
		if (command == "createFile")
		{
			string filename;
			int size;
			cout << "Enter File Name: ";
			cin >> filename;
			cout << "Enter File Size (in KB): ";
			cin >> size;
			addFile(filename, size * 1024);
			if (output == true)
				cout << "File Created Successfully\n" << endl;
			output = true;
		}

		//delete file 
		else if (command == "deleteFile")
		{
			string path;
			cout << "Enter File Path: ";
			cin >> path;
			rmFile(path);
			if (output == true)
				cout << "File Deleted Successfully\n" << endl;
			output = true;
		}

		//create directory
		else if (command == "createDir")
		{
			cout << "Enter Directory Name: ";
			cin >> parameter;
			addDir(parameter, false);
			if (output == true)
				cout << "Directory Created Successfully\n" << endl;
			output = true;
		}

		//delete directory 
		else if (command == "deleteDir")
		{
			string path;
			cout << "Enter Directory Path: ";
			cin >> path;
			rmDir(path);
			if (output == true)
				cout << "Directory Successfully Deleted\n" << endl;
			output = true;
		}

		//change directory 
		else if (command == "changeDir")
		{
			cout << "Enter New Directory Path: ";
			cin >> parameter;
			cd(parameter);
			if (output == true)
				cout << "Directory Changed Successfully\n" << endl;
			output = true;
		}

		// list of items in the current directory 
		else if (command == "dir")
		{
			ls();
		}

		//copy a file 
		else if (command == "cp")
		{
			string source, destination;
			cout << "Enter Source File Path: ";
			cin >> source;
			cout << "Enter Destination File Path: ";
			cin >> destination;
			cp(source, destination);
			if (output == true)
				cout << "File Copied Successfully \n" << endl;
			output = true;
		}

		//Display Storage Space
		else if (command == "sum")
		{
			printDiskSpaceUsage();
		}

		//Display File Contents
		else if (command == "cat")
		{
			string path;
			cout << "Enter File Name: ";
			cin >> path;
			cat(path);
		}
		//Format File System
		else if (command == "format")
		{
			format();
			if (output == true)
				cout << "Directory Formatted Successfully\n" << endl;
			output = true;
		}
		//Exit
		else if (command == "exit")
		{
			fs.close();
			return 0;
		}
		//Help
		else if (command == "help")
		{
			cout << "System commands:\n";
			cout << "\t01. Exit system....................................(exit)\n";
			cout << "\t02. Show help information..........................(help)\n";
			cout << "\t03. Create a new file........................(createFile)\n";
			cout << "\t04. Delete a file............................(deleteFile)\n";
			cout << "\t05. Create a new directory....................(createDir)\n";
			cout << "\t06. Delete a directory........................(deleteDir)\n";
			cout << "\t07. Change working directory..................(changeDir)\n";
			cout << "\t08. File list of current directory..................(dir)\n";
			cout << "\t09. Copy a file......................................(cp)\n";
			cout << "\t10. Display Storage Space...........................(sum)\n";
			cout << "\t11. Display File Contents...........................(cat)\n";
			cout << "\t12. Format File System...........................(format)\n";
		}
		else
		{
			cout << "Command not found" << endl;
		}
		fs.close();
	}
	return 0;
}
