#include <stdlib.h>
#include <iostream>
#include "pattern.h"

using namespace std;

int main()
{
    // build the vector for requests
    vector<Request> reqs;
    off_t offs[] = {0,3,7,14,17,21,28,31,35,42,46,50,54,58};
    off_t lens[] = {2,2,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    
    int n = sizeof(offs)/sizeof(off_t);
    int i;
    for ( i = 0 ; i < n ; i++ ) {
        reqs.push_back( Request( offs[i], lens[i] ) ); 
    }

    // check the requests, just to make sure
    cout << "*** Offset;Length ***" << endl;
    vector<Request>::iterator it;
    for ( it = reqs.begin() ; it != reqs.end() ; it++ ) {
        cout << it->offset << ";" << it->length << endl;
    }

    // OK, detect and show the patterns
    cout << endl;
    cout << "*** Pattern Found ***" << endl;
    RequestPatternList plist;
    PatternUtil::getReqPatList( reqs, plist );
    cout << plist.show() << endl;

    return 0;
}

