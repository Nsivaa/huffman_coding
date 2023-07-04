#ifndef UTILS_H
#define UTILS_H

#include <queue>
#include "OccurrenceMap.hpp"
#include <iostream>
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


void print_map(OccurrenceMap &words){
        cout << "MAP" << endl << endl << endl;
    for (auto i = words.beginIterator();
         i != words.endIterator(); i++){
            cout << words.getKey(i) << " " << words.getValue(i) << " " << endl;
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



void print_tree(HufNode* node, string code){
    if(node){
        if(node->character != SPECIAL_CHAR){
            cout << node->character << ":" << code << endl;
        }
        print_tree(node->left,code + "0");
        print_tree(node->right,code + "1");
    }
}

#endif
