#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include "utimer.cpp"
#include <cstring>
#include <filesystem>
#include "utils.hpp"

using namespace std;

void find_occurrences(const string &infile_name, unordered_map<char,int>& words){
    char c;
    ifstream infile;
    infile.open(infile_name);
    while(infile.get(c)){ // >> extracts single characters and puts them in string
        words[c]++; // [] operator does all the checks for us (if key is already present etc)
    }
    infile.close();
    return;
}

void map_to_queue(unordered_map<char,int> words, priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    for (unordered_map<char,int>::iterator i = words.begin();
        i != words.end(); i++){
        HufNode* node = new HufNode(i->first, i->second);
        pq.push(node);
    }
}


void generate_char_to_code_map(unordered_map<char,string> &char_code_map, HufNode* node, string code){
     if(node){
         if(node->character != SPECIAL_CHAR){
            char_code_map[node->character].append(code);
        }
        generate_char_to_code_map(char_code_map,node->left,code + "0");
        generate_char_to_code_map(char_code_map,node->right,code + "1");
    }
}

HufNode* generateTree(priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    while (pq.size() != 1) {
        HufNode* left = pq.top();
        pq.pop();
        HufNode* right = pq.top();
        pq.pop();
        //'$' as data indicates a non-leaf node
        HufNode* node = new HufNode(SPECIAL_CHAR,
                                  left->frequency + right->frequency);
        node->left = left;
        node->right = right;
        pq.push(node);
    }

    return pq.top();
}

void encode_to_file(const string& infile_name, const string &outfile_name, unordered_map<char,string> &codeMap){
	ifstream infile;
	infile.open(infile_name);
	ofstream outfile;
	outfile.open(outfile_name, ios::binary);
	uint64_t bits = 0;
	char c;
	int i=0;
	uintmax_t f=0;
	auto filesize = filesystem::file_size(infile_name);
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
	outfile.write(reinterpret_cast<const char*> (&bits), sizeof(bits)); //print the last bits that may be left in the buffer
	outfile.close();
	return;
}

void compress(const string &infile_name, const string &outfile_name){
    unordered_map<char,int> words;
    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    unordered_map<char,string> char_to_code_map;
	HufNode* huffmanTree;
	{
	utimer t1("conta occorrenze");
    find_occurrences(infile_name,words);
    }
	{
	utimer t3("map_to_queue");
	map_to_queue(words,pQueue);
	}
	{
	utimer t4("generate tree");
    huffmanTree = generateTree(pQueue);
    }
	{
	utimer t5("char_to_code_map");
	generate_char_to_code_map(char_to_code_map, huffmanTree, "");
    }
	{
	utimer t6("encode to file");
	encode_to_file(infile_name, outfile_name, char_to_code_map);
	}
}
int main(int argc, char* argv[])
/*
aggiungere controllo carattere speciale
*/
{
    if (argc < 2){
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
