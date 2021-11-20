Submitter name: Sagalpreet Singh
Roll No.: 2019csb1113
Course: CS303
=================================
1. What does this program do

This program is an implementation of a simple file system using FUSE.
A root label is created in gmail account corresponding to the mount point.
Emails act as file.
Labels as directories.

Subject of email is the filename.
Files starting with a number can't be deleted.

Since, this is a simple file system, details like creation time etc. are set to defaults.

Commands that have been tested include but are not limited to:

ls 
cd 
mkdir 
rmdir 
rm 
touch 
cat 
echo >> (for appending)
stat 
cp 
mv 
... and more

Depth of directories can be arbitrary but deeper the structure, slower the system.
Care should be taken while executing cd-like command as gmail creates some behind the scene files which may make the sytem very slow.

config file is as specified i.e:
i. Host name or the IP address of the email server. E.g., smtp.google.com
ii. Port number of the email server. E.g. 585 or 25 etc.
iii. Username to login with: some_user@gmail.com
iv. Password to login with: <whatever is the password>

There are restrictions on naming of files as posed by gmail itself.

FILE STRUCTURES
.
├── bin
│   └── main
├── config.txt
├── mnt
├── obj
├── README.txt
|---Design.pdf
├── run.sh
├── src
│   ├── helper.c
│   ├── helper.h
│   └── main.c
└── test
    ├── helper.c
    ├── helper.h
    └── main.c

=================================
2. A description of how this program works (i.e. its logic)

The file structure is built using FUSE. The skeleton code provided by libfuse repo on github is extended to fulfil the 
requirements.

How to detect if its a directory:
- Make a request to that path, if nothing is returned in the buffer string then its not a directory, else it is.

How to detect if its a file:
- Make a request to read the contents of that file (email, essentially) using imap.
- If the request fails, its not a file, else it is.
- UID of the target mail is obtained from the subject (filename) using UID SELECT query
* This puts a restriction on the possible file and directory names
* Sanity check is made that no two files or folders have similar names using SELECT query

How to create a folder:
- Simply use CREATE query to make the new label.

How to write to a file:
- Since mail once finalized can't be edited, the contents of previous mail are fetched, edited and written as a new file.
- The previous is deleted.

How to delete a file:
- Simply remove the label from corresponding email
- Then expunge it to remove, otherwise it may stay in All Mails

How to create a file:
- Simply send an empty file
* This is not exactly a send because no recepient is mentioned, neither is the sender or any other thing except subject mentioned.

How to get files in a directory:
- IMAP request UID SELECT ALL is used to get uids of all emails present at that path.
- Then corresponding subjects and content are obtained by reading those emails.

How to get sub-directories in a directory:
- The url specified carries the address of that label for which sub-directories are required.
- This returns the results as raw text in buffer which can later be manipulated.
    
=================================
3. How to compile and run this program

./run.sh

** The script has two simple commands for compiling the code and then executing it with default parameters.
** Care must be taken to edit the config file (enter email id and password there)
** Make sure, the imap settings are done for the gmail account which is being used

** Command Line arguments can be changed inside the script itself
** For reference variants are commented inside the script
