#include <iostream>
#include <fstream>
#include <unordered_map>
#include "utimer.cpp"
#include <filesystem>
#include "ThreadPool.hpp"
#include "utils.hpp"
#include "OccurrenceMap.hpp"
#include "CodeMap.hpp"
#include <chrono>
#include <future>

using namespace std;

vector<future<int>> int_futures;
vector<future<int>> map_futures;
ThreadPool *thread_pool = new ThreadPool();
mutex mut;

/*
int file_read_work(const string& infile_name){
	ifstream infile;
	infile.open(infile_name);
	vector<char> buffer (BUF
SIZE,0);
	int i=0;
	while(!infile.eof()){
		infile.read(buffer.data(),BUFSIZE);
		cout << "prepared buf n " << i << endl;
		i++;
		OccurrenceMap *thread_map = new OccurrenceMap();
		auto f = thread_pool->addJob([thread_map,buffer]()-> OccurrenceMap& {return occurrences_work(std::ref(*thread_map),std::ref(buffer));});
		{
			unique_lock<mutex> lock(mut);
			map_futures.push_back(std::move(f));
		}
	}
	infile.close();
	cout <<"file closed" << endl;
	return 1;
	}
*/

void merge_maps(OccurrenceMap &final, vector<OccurrenceMap*> maps){
	char c;
	int v;
	cout << "merging maps" << endl;
 	for(auto &m : maps){
		for(auto i = m->beginIterator(); i != m->endIterator();i++){
			c = m->getKey(i);
			v = m->getValue(i);
			final.updateValue(c,v);
		}
	}

	}

int occurrences_work(const string& infile_name, OccurrenceMap& local_map, int from, int to){
	char c;
	ifstream infile;
	infile.open(infile_name);
	int i= from;
	infile.seekg(from,ios::beg);
	while(i<to){
		infile.get(c);
		if(infile.eof()) break;
		local_map.insert(c);
		{
		unique_lock<mutex> lock(mut);
		cout << "c: " << c <<" i:" << i <<endl;
		}
		i++;
		}
	{
	unique_lock<mutex> lock(mut);
	print_map(local_map);
	}
	infile.close();
	return 1;
	}

void find_occurrences(const string &infile_name,vector <OccurrenceMap*> &maps, OccurrenceMap &words){
//master worker computes file chunks according to size and assigns work to workers
    auto size = std::filesystem::file_size(infile_name);
	int nw = thread_pool->getWorkersNumber();
	cout << "size: " << size << endl;
	int delta = size/nw;
	cout << "delta: " << delta << endl;
	for (int k=0; k<nw;k++){
		int from = k*delta;
		int to = (k == (nw-1) ? size : (k+1)*delta);
		cout << "from: " << from << endl;
		cout << "to: " << to << endl;
		OccurrenceMap *thread_map = new OccurrenceMap();
		maps.push_back(thread_map);
		auto f = thread_pool->addJob([&,thread_map,from,to]()-> int {
			 return occurrences_work(std::ref(infile_name),std::ref(*thread_map),std::move(from), std::move(to));
			});
		{
			unique_lock<mutex> lock(mut);
			int_futures.push_back(std::move(f));
		}
	}
	for (auto &f : int_futures){
		int val = f.get();
		cout <<"val:" << val << endl;
	}
	
	
	/*PRINT MAPS
	string mapfile;
	int ind =0;
	for (auto &i : maps){
	mapfile = ("print");
	mapfile.append(to_string(ind));
	mapfile.append(".txt");
	ind++;
	cout << "ind:" << ind << endl;
	ofstream file;
	file.open(mapfile);
	i->toFile(file);
	file.close();
	}*/

	merge_maps(words,maps);
	cout << "FINAL MAP" << endl;
	print_map(words);
	return;
}

void map_to_queue(OccurrenceMap &words, priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    for (auto i = words.beginIterator();
        i != words.endIterator(); i++){
        HufNode* node = new HufNode(i->first, i->second);
        pq.push(node);
    }
}

void generate_char_to_code_map(CodeMap &char_code_map, HufNode* node, string code){
     if(node){
         if(node->character != SPECIAL_CHAR){
            char_code_map.appendValue(node->character,code);
        }
        generate_char_to_code_map(char_code_map,node->left,code + "0");
        generate_char_to_code_map(char_code_map,node->right,code + "1");
    }
}

HufNode* generateTree(priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){

    while (pq.size() != 1) { //not able to vectorize
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

void compress(const string &infile_name, const string &outfile_name){
    char c;
    ifstream infile;
    ofstream outfile;
    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    CodeMap *char_to_code_map = new CodeMap();
	vector<OccurrenceMap*> maps;
	OccurrenceMap *words =  new OccurrenceMap();
	
	{
	utimer t1("conta occorrenze");
    find_occurrences(infile_name,maps,*words);

	}
	//WAIT FOR THREADS TO FINISH
		
	for (auto &f : map_futures){
		auto val = f.get();
	}
	cout << "Maps size:" << maps.size() << endl;
	//merge_maps(*words,maps);
	string outname = to_string(thread_pool->getWorkersNumber());
	outname.append("map.txt");
	ofstream out_map_file;
	out_map_file.open(outname);
	words->toFile(out_map_file);
	map_to_queue(*words,pQueue);
    //print_queue(pQueue);
    HufNode* huffmanTree = generateTree(pQueue);
    //print_tree(huffmanTree,"");
    generate_char_to_code_map(*char_to_code_map, huffmanTree, "");
    //print_map(char_to_code_map);
    infile.open(infile_name);
    outfile.open(outfile_name);

    while(infile >> c){
        outfile << (char_to_code_map->getCode(c));
    }
    infile.close();
    outfile.close();
}

int main(int argc, char* argv[])
/*
aggiungere controllo carattere speciale
*/
{
    if (argc < 3){
        cout << "Please provide arguments! ";
        return EXIT_FAILURE;
    }
    int nw = (argc > 3 ? atoi(argv[3]) : 0);   // par degree

    {
	utimer t1("thread spawning");
	thread_pool->start(nw);
    }
    string infile_name = (argv[1]);
    string outfile_name = (argv[2]);

    {
        utimer t0("whole program");
        compress(infile_name, outfile_name);
    }
	thread_pool->stop();
    return 0;
}

