#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include "huf_sequential.h"

using namespace std;
#define SPECIAL_CHAR '$'

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

void find_occurrences(istream& stream, unordered_map<char,int>& words){
    char c;
    while(stream >> c){ // >> extracts single words and puts them in string
        words[c]++; // [] operator does all the checks for us (if key is already present etc)
    }
    return;
}

void print_map(unordered_map<char,int> &words ){
        cout << "MAP" << endl << endl << endl;
    for (unordered_map<char,int>::iterator i = words.begin();
         i != words.end(); i++){
            cout << i->first << " occurred: " << i->second << "times " << endl;
         }
        return;
}

void print_queue(priority_queue<HufNode*,vector<HufNode*>, Compare> pq){ //we pass pq by value so that pop() doesn't actually delete
    cout << "QUEUE" << endl << endl << endl;
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

void print_tree(HufNode* root, string code){
    if(root){
        if(root->character != SPECIAL_CHAR){
            cout << root->character << ":" << code << endl;
        }
        print_tree(root->left,code + "0");
        print_tree(root->right,code + "1");
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

int main(int argc, char* argv[]) //aggiungere controllo carattere speciale
{
    cout << argc << endl;
    if (argc < 2){
        return EXIT_FAILURE;
    }
     ifstream file(argv[1]);
     if (!file){
         exit(EXIT_FAILURE);
    }
    unordered_map<char,int> words;
    find_occurrences(file,words);
    print_map(words);
    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    map_to_queue(words,pQueue);
    print_queue(pQueue);
    HufNode* huffmanTree = generateTree(pQueue);
    print_tree(huffmanTree,"");

    return 0;
}
