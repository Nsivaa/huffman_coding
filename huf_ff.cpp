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

#define BUFSIZE 2048

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
	PipeTask(unordered_map<char,string> &cMap, ofstream &stream, vector<char> &buf) :
	codeMap(cMap), outfile(stream), file_buffer(buf) {}
	unordered_map<char,string> codeMap;
	ofstream &outfile;
	vector<char> file_buffer;
	string huf_codes;
	inline static uint64_t bits;
	inline static short int written_bits;
	inline static unsigned long int pos;
};

struct Source : ff_node_t<PipeTask>{

	Source(ifstream &in, ofstream &out, unordered_map<char,string> &cMap) :
			infile(in), outfile(out), codeMap(cMap) {}

	PipeTask * svc(PipeTask *){
		char c;
		int i=0;
		vector<char> buffer;
		while(true){
			i=0;
			while(i<BUFSIZE){
				if(infile.get(c)){
					buffer.push_back(c);
					i++;
				}
				else return (EOS);
			}
			PipeTask *t2 = new PipeTask(codeMap, outfile,buffer);
			buffer.clear();//useless?
			ff_send_out(t2);
	}
	return (EOS);
}

	ifstream& infile;
	ofstream& outfile;
	unordered_map<char,string> codeMap;
};

struct FirstStage: ff_node_t <PipeTask>{
	PipeTask * svc(PipeTask *t){
		for (auto &i : t->file_buffer){
			t->huf_codes.append(t->codeMap[i]); //pre-allocate huf_codes?
		}
		return t;
	}
};

//TODO: IMPLEMENT FEEDBACK TO WRITE LAST NON-FULL BUFFER?

struct LastStage : ff_node_t <PipeTask>{
	PipeTask * svc(PipeTask *t){
		for(auto &c : t->huf_codes){
			if(t->written_bits == 64){
				t->outfile.seekp(t->pos,ios::beg);
				t->outfile.write(reinterpret_cast<const char*>(&t->bits), sizeof(t->bits));
				t->bits = 0;
				t->written_bits=0;
				t->pos += sizeof(t->bits);
			}
			if(c == '1') t->bits |= 1 << (63 - t->written_bits );
			t->written_bits++;
		}
	return (GO_ON);
	}
};

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
	utimer t6("encode to file pipeline");
		ifstream infile;
		infile.open(infile_name);
		ofstream outfile;
		outfile.open(outfile_name,ios::binary);
		Source s1(infile, outfile, char_to_code_map);
		FirstStage s2;
		LastStage s3;
		ff_Pipe<PipeTask> encode_pipeline(s1,s2,s3);
		encode_pipeline.run_and_wait_end();
		encode_pipeline.ffStats(cout);
		outfile.close();
	}
}

int main(int argc, char* argv[]){
    if (argc < 4){
        cout << "Usage is [input txt file] [output binary file name] [nw]" << endl;
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
