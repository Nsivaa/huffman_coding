#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include <cstring>
#include <filesystem>

#include "utils.hpp"
#include "utimer.cpp"

using namespace std;

void find_occurrences(vector<char> &data, unordered_map<char,int> &occurrenceMap){	/* 
	 * fills the unordered map with characters from the file and their occurrences
	*/
    char c;
	
	
	for (auto &i : data){
		occurrenceMap[i]++;

	}

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

void encode_to_file(string data, const string &outfile_name, unordered_map<char,string> &codeMap){
	/*
 	* Opens the input txt file and translates every character into its corresponding huffman code.
	* We use a 64 bit variable initialized to 0 as buffer and perform a bitwise shift whenever we need to write a '1'.
	* If we need a '0' we just skip to the next bit (we don't need to shift).
	*/

	uint64_t bits = 0;
	char c;
	int i=0;
	uintmax_t f=0;
	vector<uint64_t> output;

	

	c=data[0];
	string byte_str = codeMap[c];
	auto iter = byte_str.begin();

	while(f<data.size()){ 
		if(iter == byte_str.end()){
			c = data[f];
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
			output.push_back(bits);
			bits=0;
			i=0;
		}
	}
	output.push_back(bits);
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
	ifstream infile;
	infile.open(infile_name);
	vector<char> data;
	char c;
	while(infile.get(c)){
		data.push_back(c);
	}
	infile.close();
	{
 utimer t0("whole program");
	{
	utimer t1("count occurrences");
    find_occurrences(data,occurrenceMap);
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
	string data2;
	infile.open(infile_name);
	char ch;
	while(infile.get(ch)){
		data2.append(codeMap[ch]);
	}
	infile.close();
	{
	utimer t6("encode to file");
	encode_to_file(data2,outfile_name, codeMap);
	}
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
    
        
        compress(infile_name, outfile_name);
   

    return 0;
}
