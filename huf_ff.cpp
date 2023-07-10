#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include "utimer.cpp"
#include <cstring>
#include <filesystem>
#include "utils.hpp"
#include <ff/ff.hpp>
#include <ff/parallel_for.hpp>

#define BUFSIZE 1024

using namespace std;
using namespace ff;

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

void find_occurrences(const string &infile_name,unordered_map<char,int> &words, int nw){
    auto size = std::filesystem::file_size(infile_name);
	vector <char> data (size);
	ifstream infile;
	infile.open(infile_name);
	{
		utimer t0("vector filling");
		char c;
		int i=0;
		while(infile.get(c)){
			data[i]=c;
			i++;
		}
	}
	auto ReduceF = [&](unordered_map<char,int> &m,const unordered_map<char,int> m2){
		char c;
		for (auto &i : m2){
		c=i.first;
		m[c]+=i.second;
		}
	};
	unordered_map<char,int> Wzero;
	{
		utimer t1("counting occurrences");
		ParallelForReduce<unordered_map<char,int>> pf(nw);
		pf.parallel_reduce(words,Wzero,0,size,1,0, [&] (const long i, unordered_map<char,int> &mymap) {
			char c;
			c=data[i];
			mymap[c]++;
		}, ReduceF);
	}
	infile.close();
	return;
}

struct PipeTask{
	const string infile_name;
	const string outfile_name;
	unordered_map<char,string> codeMap;
	vector<char> file_buffer;
	string code_buffer;
};

struct Source : ff_node_t<PipeTask>{
	PipeTask * svc(PipeTask *t){
		char c;
		int i=0;
		ifstream infile;
		infile.open(t->infile_name);
		while(true){
			while(i<BUFSIZE){
				if(infile.get(c)){
					t->file_buffer.push_back(c);
					i++;
				}
				else{
					return (EOS);
				}
			}
			ff_send_out(t);
			i=0;
			t->file_buffer.clear();
		}
	}
};

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

void compress(const string &infile_name, const string &outfile_name, int nw){
    unordered_map<char,int> words;
    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    unordered_map<char,string> char_to_code_map;
	HufNode* huffmanTree;
	{
    find_occurrences(infile_name,words,nw);
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
int main(int argc, char* argv[]){
    if (argc < 2){
        cout << "Usage is [input txt file] [output binary file name] [nw]";
        return EXIT_FAILURE;
    }
    string infile_name = (argv[1]);
    string outfile_name = (argv[2]);
	int nw = atoi(argv[3]);
    {
        utimer t0("whole program");
        compress(infile_name, outfile_name,nw);
    }

    return 0;
}
