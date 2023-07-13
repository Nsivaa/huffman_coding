#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include <cstring>
#include <filesystem>

#include "utils.hpp"
#include "utimer.cpp"

using namespace std;

void find_occurrences(const string &infile_name, unordered_map<char,int>& occurrenceMap){
	/* 
	 * fills the unordered map with characters from the file and their occurrences
	*/
    char c;
    ifstream infile;
    infile.open(infile_name);
    while(infile.get(c)){ // >> extracts single characters and puts them in string
        occurrenceMap[c]++; // [] operator does all the checks for us (if key is already present etc)
    }
    infile.close();
    return;
}

void map_to_pQueue(unordered_map<char,int> occurrenceMap, priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    /*
	 * Creates nodes from the map elements (char-freq) and stores them in the priority queue.
	 * The queue holds items in increasing order of occurrence.
	*/
	
	for (unordered_map<char,int>::iterator i = occurrenceMap.begin();
        i != occurrenceMap.end(); i++){
        HufNode* node = new HufNode(i->first, i->second);
        pq.push(node);
    }
}


void generate_codeMap(unordered_map<char,string> &char_code_map, HufNode* node, string code){
    /*
 	* Generates a map which contains pairs of characters and their relative huffman code
	*/

	 if(node){ // base case would be node==NULL since we initialised them this way
         if(node->character != SPECIAL_CHAR){
            char_code_map[node->character].append(code);
        }
        generate_codeMap(char_code_map,node->left,code + "0");
        generate_codeMap(char_code_map,node->right,code + "1");
    }
}

HufNode* generateTree(priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
	/*
 	* generates the Huffman tree. 'SPECIAL_CHAR' as data indicates a non-leaf node.
	*/

    while (pq.size() != 1) { 
        HufNode* left = pq.top();
        pq.pop();
        HufNode* right = pq.top();
        pq.pop();
        HufNode* node = new HufNode(SPECIAL_CHAR,
                                  left->frequency + right->frequency);
        node->left = left;
        node->right = right;
        pq.push(node);
    }

    return pq.top(); //last remaining node is the root of the huffman tree
}

void encode_to_file(const string& infile_name, const string &outfile_name, unordered_map<char,string> &codeMap){
	/*
 	* Opens the input txt file and translates every character into its corresponding huffman code.
	* We use a 64 bit variable initialized to 0 as buffer and perform a bitwise shift whenever we need to write a '1'.
	* If we need a '0' we just skip to the next bit (we don't need to shift).
	*/

	uint64_t bits = 0;
	char c;
	int i=0;
	uintmax_t f=0;
	auto filesize = filesystem::file_size(infile_name);

	ifstream infile;
	infile.open(infile_name);
	ofstream outfile;
	outfile.open(outfile_name, ios::binary);

	infile.get(c);
	string byte_str = codeMap[c];
	auto iter = byte_str.begin();

	while(f<filesize){ 
		if(iter == byte_str.end()){
			infile.get(c);
			byte_str = codeMap[c];
			iter = byte_str.begin();
			f++;
		}
		while(i<64 && iter != byte_str.end()){
			if( *iter == '1' ) {
				bits |= 1 << (63-i);
			}
			iter++;
			i++;
		}
		if (i==64){
			outfile.write(reinterpret_cast<const char *>(&bits), sizeof(bits));
			bits=0;
			i=0;
		}
	}
	outfile.write(reinterpret_cast<const char*> (&bits), sizeof(bits)); //print the last bits that may be left in the buffer.
	outfile.close();
	return;
}

void compress(const string &infile_name, const string &outfile_name){
    /*
 	* Main procedure of the program. Creates the data structures and calls all the functions of the application, timing their execution.
	*/
	unordered_map<char,int> occurrenceMap;
    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    unordered_map<char,string> codeMap;
	HufNode* huffmanTree;
	{
	utimer t1("count occurrences");
    find_occurrences(infile_name,occurrenceMap);
    }
	{
	utimer t3("map_to_queue");
	map_to_pQueue(occurrenceMap,pQueue);
	}
	{
	utimer t4("generate tree");
    huffmanTree = generateTree(pQueue);
    }
	{
	utimer t5("codeMap");
	generate_codeMap(codeMap, huffmanTree, "");
    }
	{
	utimer t6("encode to file");
	encode_to_file(infile_name, outfile_name, codeMap);
	}
}


int main(int argc, char* argv[])
{
    if (argc < 3){
        cout << "Usage is [input txt file] [output binary file name]";
        return EXIT_FAILURE;
    }
    string infile_name = (argv[1]);
    string outfile_name = (argv[2]);
    {
        utimer t0("whole program");
        compress(infile_name, outfile_name);
    }

    return 0;
}
