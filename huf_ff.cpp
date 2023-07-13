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

#define BUFSIZE 8192

using namespace std;
using namespace ff;

void map_to_queue(unordered_map<char,int> words, priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    /*
	 * Creates nodes from the map elements (char-freq) and stores them in the priority queue.
	 * The queue holds items in increasing order of occurrence.
	*/
    for (unordered_map<char,int>::iterator i = words.begin();
        i != words.end(); i++){
        HufNode* node = new HufNode(i->first, i->second);
        pq.push(node);
    }
}


void generate_codeMap(unordered_map<char,string> &char_code_map, HufNode* node, string code){
    /*
 	* Generates a map which contains pairs of characters and their relative huffman code
	*/
     if(node){
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
    return pq.top();
}

void find_occurrences(const string &infile_name,unordered_map<char,int> &words, int nw){
    /*
 	* Occurrence counting is obtained with a ParallelForReduce. The downside is that a way to operate
 	* directly on the file was not found. therefore, we are transferring the file content to a vector
 	* and the operation is performed on it.
	*/
    auto size = std::filesystem::file_size(infile_name);
	vector <char> data (size);
	ifstream infile;
	infile.open(infile_name);
	{
		utimer t0("vector filling"); //transferring data to the vector
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
	unordered_map<char,int> Wzero; //identity collection
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
    /*
 	* Structure of the task brought on by the pipeline stages
	*/
	PipeTask(unordered_map<char,string> &cMap, ofstream &stream, vector<char> &buf) :
	codeMap(cMap), outfile(stream), file_buffer(buf) {}
	unordered_map<char,string> codeMap;
	ofstream &outfile;
	vector<char> file_buffer;
	string huf_codes; //string and not vector<char> so that we can append whole strings (huf codes) to it
	queue<uint64_t> full_bits;

	//static so their value is mantained between different pipe cycles
	inline static uint64_t bits_buffer=0;
	inline static short int written_bits=0;
	inline static unsigned long int pos=0;
	inline static int written=0;
};

struct Source : ff_node_t<PipeTask>{

	Source(ifstream &in, ofstream &out, unordered_map<char,string> &cMap) :
			infile(in), outfile(out), codeMap(cMap) {}
    /*
 	* Source of the pipeline. Reads chars from the input file and sends them via
 	* buffers to the next stages
	*/
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
				else{
				PipeTask *t2 = new PipeTask(codeMap,outfile,buffer);
				ff_send_out(t2);
				buffer.clear();
				return (EOS);
				}
			}
			PipeTask *t2 = new PipeTask(codeMap,outfile,buffer);
			ff_send_out(t2);
			buffer.clear();
	}
	return (EOS); //never reached
}

	ifstream& infile;
	ofstream& outfile;
	unordered_map<char,string> codeMap;
};

struct FirstStage: ff_node_t <PipeTask>{
    /*
 	* Simply translates chars into huf codes
	*/
	PipeTask * svc(PipeTask *t){
		for (auto &i : t->file_buffer){
			t->huf_codes.append(t->codeMap[i]); //pre-allocate huf_codes?
		}
		return t;
	}
};

struct SecondStage : ff_node_t <PipeTask>{
    /*
 	* Receives huf codes from the previous stage (via a single string) and translates
 	* its content into 64-bit buffers. They are stored in a queue which is sent to the
 	* last stage.
	*/
	PipeTask * svc(PipeTask *t){
		for(auto &c : t->huf_codes){
			if(c == '1') t->bits_buffer |= 1 << (63 - t->written_bits );
			t->written_bits++;
			if(t->written_bits == 64){
				t->full_bits.push(t->bits_buffer);
				t->bits_buffer = 0;
				t->written_bits=0;
			}
		}
		return t;
	}
};

struct LastStage : ff_node_t <PipeTask>{
    /*
 	* Last Stage, simply pops the binary buffers from the queue and writes them to the file.
	*/
	PipeTask * svc(PipeTask *t){
		int written=0;
		t->outfile.seekp(t->pos,ios::beg);
		while(!t->full_bits.empty()){
			uint64_t data = t->full_bits.front();
			t->full_bits.pop();
			t->outfile.write(reinterpret_cast<const char*>(&data), sizeof(data));
			written++;
		}
	t->pos += written * sizeof(uint64_t); //we keep track of the position for the next pipe cycles
	return (GO_ON);
	}

};

void compress(const string &infile_name, const string &outfile_name, int nw){
    /*
 	* Main procedure of the program. Creates the data structures and calls all the functions of the application, timing their execution.
	*/
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
	utimer t5("generate code_map");
	generate_codeMap(char_to_code_map, huffmanTree, "");
    }

	{
	utimer t6("encode to file pipeline");
	/*
 	* Input and output streams are instantiated here and their reference passed to the pipeline. In this way we
 	* both reduce the size of the tasks (less fields in the struct), and we are able to close the output file at the end of
 	* the writing process. Also, we avoid opening and closing the file at every task
	*/
		ifstream infile;
		infile.open(infile_name);
		ofstream outfile;
		outfile.open(outfile_name,ios::binary);
		Source s1(infile, outfile, char_to_code_map);
		FirstStage s2;
		SecondStage s3;
		LastStage s4;
		ff_Pipe<PipeTask> encode_pipeline(s1,s2,s3,s4);
		encode_pipeline.run_and_wait_end();
		encode_pipeline.ffStats(cout);
		infile.close();
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
