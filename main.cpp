#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include "huf_sequential.h"
#include "utimer.cpp"
#include <cstring>

using namespace std;
#define SPECIAL_CHAR '$' //non-leaf nodes

class HufNode{
public:
    HufNode(char c, int f) : character(c), frequency(f), left(NULL), right(NULL){};

friend ostream& operator<<(ostream& os, const HufNode* node) {
        return os << "character: " << node->character << endl << "frequency: " << node->frequency<< endl;
    }

    char character;
    int frequency;
    HufNode* left;
    HufNode* right;
};

class Compare {
    public:
    bool operator()(HufNode* a,
                    HufNode* b)
    {
        return a->frequency > b->frequency;
    }
};

void find_occurrences(const string &infile_name, unordered_map<char,int>& words){
    char c;
    ifstream infile;
    infile.open(infile_name);
    while(infile >> c){ // >> extracts single words and puts them in string
        words[c]++; // [] operator does all the checks for us (if key is already present etc)
    }
    infile.close();
    return;
}

template<typename T, typename V>
void print_map(unordered_map<T,V> &words ){
        cout << "MAP" << endl << endl << endl;
    for (typename unordered_map<T, V>::iterator i = words.begin();
         i != words.end(); i++){
            cout << i->first << " " << i->second << " " << endl;
         }
        return;
}

void print_queue(priority_queue<HufNode*,vector<HufNode*>, Compare> pq){ //we pass pq by value so that pop() doesn't actually delete
    cout << "QUEUE" << endl;
    while (! pq.empty() ) {
    cout << pq.top() << "\n";
    pq.pop();
    }
}

void map_to_queue(unordered_map<char,int> words, priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    for (unordered_map<char,int>::iterator i = words.begin();
        i != words.end(); i++){
        HufNode* node = new HufNode(i->first, i->second);
        pq.push(node);
    }
}

void print_tree(HufNode* node, string code){
    if(node){
        if(node->character != SPECIAL_CHAR){
            cout << node->character << ":" << code << endl;
        }
        print_tree(node->left,code + "0");
        print_tree(node->right,code + "1");
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

void compress(const string &infile_name, const string &outfile_name){
    char c;
    ifstream infile;
    ofstream outfile;
    unordered_map<char,int> words;
    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    unordered_map<char,string> char_to_code_map;

    find_occurrences(infile_name,words);
    map_to_queue(words,pQueue);
    //print_queue(pQueue);
    HufNode* huffmanTree = generateTree(pQueue);
    //print_tree(huffmanTree,"");
    generate_char_to_code_map(char_to_code_map, huffmanTree, "");
    //print_map(char_to_code_map);
    infile.open(infile_name);
    outfile.open(outfile_name);

    while(infile >> c){
        outfile.write(reinterpret_cast<const char*>(&char_to_code_map[c]), sizeof(char_to_code_map[c]));
    }
    infile.close();
    outfile.close();
}
int main(int argc, char* argv[])
/*
aggiungere controllo carattere speciale
*/
{
    if (argc < 2){
        cout << "Please provide arguments! ";
        return EXIT_FAILURE;
    }
    string infile_name = (argv[1]);
    string outfile_name = (argv[2]);
    {
        utimer t0("prova");
        compress(infile_name, outfile_name);
    }

    return 0;
}
