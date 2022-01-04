#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

// cd - stopped working

// trim function for unwanted spaces before, between, and after
string trim (string input) {
	if (input.empty()) {
		return "";
	}
	// whitespace before commands
	while (isspace(input[0])) {
		input.erase(input.begin());
	}
	// whitespace between commands
	size_t count = 0;
	while (count != (input.length() - 2)) {
		if (isspace(input[count]) && isspace(input[count + 1])) {
			input.erase(input.begin() + count);
		} else {
			count++;
		}
	}
	// whitespace after commands
	while (isspace(input[input.length() - 1])) {
		input.pop_back();
	}
	return input;
}

// give vector of strings
// make array of char pointers inside
// each entry will point to the string converted as char pointer
char** vec_to_char_array (vector<string>& parts) {
	char** result = new char* [parts.size() + 1];  // ** cuz array, each item is a char pointer, +1 because exec fxns expects a NULL terminated list of strings
	
	for (int i = 0; i < parts.size(); i++) {
		result[i] = (char*) parts [i].c_str();  // convert each item in the vector
	}

	result[parts.size()] = NULL;
	return result;
}

// implement to split up arguments, given string it returns a vector of strings
vector<string> split (string line, string separator=" ") {
	size_t pos = 0;
	vector<string> parts;

	while ((pos = line.find(separator)) != string::npos) {  // finds position of next space if none left npos==npos
		parts.push_back (line.substr(0, pos));  // push back sections of string
		line.erase (0, pos + separator.length());  // erase the line worked so far
	}

	parts.push_back (line.substr(0));  // Add the last string since there is no spaces after it
	return parts;
}

int findCommand (char** args, char command) {
	char** temp = args;
	int count = 0;
	while (*temp != nullptr) {
		if (**temp == command) {
			return count;
		}
		count++;
		temp++;
	}
	return -1;
}

int getSize (char** args) {
	char** temp = args;
	int count = 0;
	while (*temp != nullptr) {
		count++;
		temp++;
	}
	return count;
}

char** deleteRedirection (char** args, int index) {
	int size = getSize(args);
	char** result = new char* [size - 1];  // not - 2 because exec needs null so - 1
	for (int i = 0; i < size - 2; i++) {
		result[i] = args[i];  // Only get the lines before redirection
	}
	result[size - 2] = NULL;  // must set last index as null
	return result;
}

char** fileRedirection (char** args) {
	// input/output redirection
	// find index of <, or > in vector and the next index is the file
	int inDirect = findCommand(args, '<');  // reads from file
	int outDirect = findCommand(args, '>');  // writes to file
	if (outDirect != -1) {
		int fd = open(args[outDirect + 1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		dup2(fd, 1);  // overwrite stdout with new file
		close(fd);
		args = deleteRedirection(args, outDirect);
	}
	if (inDirect != -1) {  // input
		int fd = open(args[inDirect + 1], O_CREAT|O_RDONLY, S_IRUSR|S_IRGRP|S_IROTH);
		dup2(fd, 0);  // overwrite stdout with new file
		close(fd);
		args = deleteRedirection(args, outDirect);
	}
	return args;
}

// ps aux | awk '/usr/{print $1}' | sort -r
string deleteQuotes (string input, int index) {
	string tempLine = "";
	if (index == 4 && (input.find("/") == string::npos)) {  // awk
		int pos = input.find("{");
		input.erase(input.begin() + pos - 1);
		pos = input.find("}") + 1;
		input.erase(input.begin() + pos);
		if (input[pos] == '<') {
			input.insert(pos, " ");
			input.insert((pos + 2), " ");
		}
		tempLine = input;
	} else {  // not awk
		tempLine = input.substr(0, index++);
		input.pop_back();
		tempLine += input.substr(index);
	}
	return tempLine;
}

void execute (string command) {
	if (command.find("awk") != std::string::npos) {
		command = deleteQuotes(command, 4);
	}
	vector<string> parts = split (command);
	for (int i = 0; i < parts.size(); i++) {
		if (parts[i].find("{") != string::npos) {
			parts[i] += parts[i + 1];
			parts.erase(parts.begin() + i + 1);
			break;
		}
	}
	char** args = vec_to_char_array (parts);
	args = fileRedirection(args);
	execvp (args[0], args);
}

int main (){
	vector<int> bgs;  // vector of back ground processes

	// "save the default values of stdin and stdout before the while loop starts and it gets corrupted"
	int stdin = dup(0);  // stdin
	int stdout = dup(1);  // stdout
	string previousDirectory;  // remember previous working directory
	string previousWorkingDirectory;
	bool rememberDirectory = true;

	while (true) {

		// check background processes before next command
		for (int i = 0; i < bgs.size(); i++) {  // reap zombies from the dead
			// need to check background process without making the entire shell idle
			if (waitpid (bgs[i], 0, WNOHANG) == bgs[i]) {  // returns process id if it is done waiting
				cout << "Process: " << bgs[i] << " ended" << endl;
				bgs.erase (bgs.begin() + i);
				i--;  // keep i at same spot after erase
			}
		}

		// reset dups before any cins and couts
		dup2(stdin, 0);
		dup2(stdout, 1);
		
		// prompt
		cout << getenv("USER") << "$ ";
		// cout << getuid() << "$ ";  
		string inputline;
		getline (cin, inputline);

		if(inputline == string ("exit")){
			cout << "Bye!! End of shell" << endl;
			return 0;  // his text is break
			break;
		}

		// background processes have a & in it, need to account for that and handle it
		bool bg = false;

		inputline = trim(inputline);
		if (inputline.find("&") != std::string::npos) {
			bg = true;
			inputline = inputline.substr (0, inputline.size()-1);  // remove & from end of string
		}
		inputline = trim(inputline);

		// Change directory
		if (inputline.find("cd") != std::string::npos) {
			string path = inputline.substr(3); 
			if (path == "-") {
				path = previousWorkingDirectory;
			}
			chdir(path.c_str());
			// Remembers last working directory
			if (rememberDirectory == true) {
				previousDirectory = path;
				rememberDirectory = false;
			} else {
				previousWorkingDirectory = previousDirectory;
				previousDirectory = path;
				rememberDirectory = true;
			}
			continue;
		}

		// if no pipes or there is echo, run here
		if (inputline.find("|") == std::string::npos || inputline.find("echo") != std::string::npos) {
			int pid = fork ();

			if (pid == 0) { //child process
				if (inputline.find("echo") != std::string::npos) {
					if (inputline.find("-e") != std::string::npos) {  // echo -e
						inputline = deleteQuotes(inputline, 8);
					} else {  // echo 
						inputline = deleteQuotes(inputline, 5);
					}
				}

				// preparing the input command for execution
				vector<string> parts = split (inputline);
				char** args = vec_to_char_array (parts);

				args = fileRedirection(args);

				execvp (args[0], args);  // feed array into execvp, takes all arguments in command line and execute properly
			}
			else {
				if (!bg) {  // only wait for background processes
					waitpid (pid, 0, 0); //parent waits for child process
				}
				else {
					bgs.push_back (pid);  // so we don't lose track of the background process
				}
			}
		} else {  // pipes
			vector<string> pipeParts = split (inputline, "|");
			
			for (int i = 0; i < pipeParts.size(); i++){
				// set up the pipe
				int fd [2];
				pipe (fd);

				// Create child process
				int cid = fork();

				if (!cid) { // in the child process
					// redirect output to the next level unless this is the last level
					if (i < pipeParts.size() - 1) {
						dup2(fd[1], 1);  // redirect STDOUT to fd[1], so that it can write to the other side
					}
					execute (trim(pipeParts[i]));  // execute the command at this level
				} else { // in the parent process
					if (i == pipeParts.size() - 1) {
						waitpid (cid, 0, 0);  //1. wait for the child process running the current level command
					}
					dup2(fd[0], 0);  // redirect input from the child process
					close (fd[1]);  // fd[1] MUST be closed, otherwise the next level will wait
				}
			}
		}
	}
}