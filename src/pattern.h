#ifndef __pattern_h__
#define __pattern_h__

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <sstream>
#include <stdint.h>
#include <string.h>


// Pattern Family (from small to big):
//   PatternUnit
//   PatternElem
//   PatternChain

template <class T> class PatChain;
template <class T> class PatternChain;

using namespace std;

typedef int32_t header_t; //the type to hold the body size in serialization

class Request {
    public:
        off_t offset;
        off_t length;

        Request();
        Request(off_t off, off_t len);
};

Request::Request(off_t off, off_t len)
    : offset(off), 
      length(len)
{}

Request::Request()
    : offset(0),
      length(0)
{}

// Used to describe a single pattern that is found
// This will be saved in the stack.
// For example, (seq)^cnt=(3,5,8)^2
class PatternUnit {
    public:
        vector<off_t> seq;
        int64_t cnt; //count of repeatition
        
        PatternUnit() {}
        PatternUnit( vector<off_t> sq, int ct )
            :seq(sq),cnt(ct)
        {}
        
        int size() const;
        virtual string show() const;
};


// This is used to describ a single repeating
// pattern, but with starting value
class PatternElem: public PatternUnit {
    public:
        off_t init; // the initial value of 
                    // logical offset, length, or physical offset
        string show() const;

        header_t bodySize();
        string serialize();
        void deSerialize(string buf);
        off_t getValByPos( const int &pos ) ;
        bool append( PatternElem &other );
        bool isSeqRepeating();
        void compressRepeats();
};



template <class T> // T can be PatternUnit or PatternElem
class PatternChain {
    public:
        PatternChain() {}
        void push( T pu ); 
        void clear() ;
        bool isPopSafe( int t ); 
        bool popElem ( int t );
        //pop out one pattern
        void popPattern ();
        
        //make sure the stack is not empty before using this
        T top ();
        typename vector<T>::const_iterator begin() const;
        typename vector<T>::const_iterator end() const;
        virtual string show();
        int bodySize();
        string serialize();    
        void deSerialize( string buf );
        
        vector<T> chain;
};


void appendToBuffer( string &to, const void *from, const int size );
void readFromBuf( string &from, void *to, int &start, const int size );
off_t sumVector( vector<off_t> seq );

////////////////////////////////////////////////////////////////////////
// Template functions
////////////////////////////////////////////////////////////////////////


// Serilize a PatternChain to a binary stream
template <class T>
string 
PatternChain<T>::serialize()
{
    header_t bodysize = 0;
    string buf;
    typename vector<T>::iterator iter;
    header_t realtotalsize = 0;

    bodysize = bodySize();
    //cout << "data size put in: " << bodysize << endl;
    appendToBuffer(buf, &bodysize, sizeof(bodysize));
    for ( iter = chain.begin() ; 
            iter != chain.end() ;
            iter++ )
    {
        string unit = iter->serialize();
        appendToBuffer(buf, &unit[0], unit.size());
        realtotalsize += unit.size();
        //to test if it serilized right
        //PatternElem tmp;
        //tmp.deSerialize(unit);
        //cout << "test show.\n";
        //tmp.show();
    }
    assert(realtotalsize == bodysize);
    return buf;
}

template <class T>
void
PatternChain<T>::deSerialize( string buf )
{
    header_t bodysize, bufsize;
    int cur_start = 0;
    
    clear(); 

    readFromBuf(buf, &bodysize, cur_start, sizeof(bodysize));
    
    bufsize = buf.size();
    assert(bufsize == bodysize + sizeof(bodysize));
    while ( cur_start < bodysize ) {
        header_t unitbodysize;
        string unitbuf;
        T patunit;

        readFromBuf(buf, &unitbodysize, cur_start, sizeof(unitbodysize));
        //cout << "Unitbodysize:" << unitbodysize << endl;
        int sizeofheadandunit = sizeof(unitbodysize) + unitbodysize;
        unitbuf.resize(sizeofheadandunit);
        if ( unitbodysize > 0 ) {
            //TODO:make this more delegate
            cur_start -= sizeof(unitbodysize);
            readFromBuf(buf, &unitbuf[0], cur_start, sizeofheadandunit); 
        }
        patunit.deSerialize(unitbuf);
        push(patunit);
    }

}


template <class T>
int
PatternChain<T>::bodySize()
{
    int totalsize = 0;
    typename vector<T>::iterator iter;
    for ( iter = chain.begin() ; 
            iter != chain.end() ;
            iter++ )
    {
        //PatternElem body size and its header
        totalsize += (iter->bodySize() + sizeof(header_t));
    }
    return totalsize;
}

template <class T>
void 
PatternChain<T>::push( T pu ) 
{
    chain.push_back(pu);
}

template <class T>
void
PatternChain<T>::clear() 
{
    chain.clear();
}

// if popping out t elements breaks any patterns?
template <class T>
bool
PatternChain<T>::isPopSafe( int t ) 
{
    typename vector<T>::reverse_iterator rit;
    
    int total = 0;
    rit = chain.rbegin();
    while ( rit != chain.rend()
            && total < t )
    {
        total += rit->size();
        rit++;
    }
    return total == t;
}

// return false if it is not safe
// t is number of elements, not pattern unit
template <class T>
bool
PatternChain<T>::popElem ( int t )
{
    if ( !isPopSafe(t) ) {
        return false;
    }

    int total = 0; // the number of elem already popped out
    while ( !chain.empty() && total < t ) {
        total += top().size();
        chain.pop_back();
    }
    assert( total == t );

    return true;
}

//pop out one pattern
template <class T>
void
PatternChain<T>::popPattern () 
{
    chain.pop_back();
}

//make sure the stack is not empty before using this
template <class T>
T
PatternChain<T>::top () 
{
    assert( chain.size() > 0 );
    return chain.back();
}

template <class T>
typename vector<T>::const_iterator
PatternChain<T>::begin() const
{
    return chain.begin();
}

template <class T>
typename vector<T>::const_iterator
PatternChain<T>::end() const
{
    return chain.end();
}

template <class T>
string 
PatternChain<T>::show()
{
    typename vector<T>::const_iterator iter;
    ostringstream showstr;

    for ( iter = chain.begin();
            iter != chain.end();
            iter++ )
    {
        showstr << iter->show();
        /*
        vector<off_t>::const_iterator off_iter;
        for ( off_iter = (iter->seq).begin();
                off_iter != (iter->seq).end();
                off_iter++ )
        {
            cout << *off_iter << ", ";
        }
        cout << "^" << iter->cnt << endl;
        */
    }
    return showstr.str();
}


template <class T>
class PatChain: public PatternChain <T> 
{
    public:
        virtual string show()
        {
            typename vector<T>::const_iterator iter;
            ostringstream showstr;
            for ( iter = this->chain.begin();
                    iter != this->chain.end();
                    iter++ )
            {
                vector<off_t>::const_iterator off_iter;
                showstr << iter->init << "- (" ;
                for ( off_iter = (iter->seq).begin();
                        off_iter != (iter->seq).end();
                        off_iter++ )
                {
                    showstr << *off_iter << ", ";
                }
                showstr << ")^" << iter->cnt << endl;
            }
            return showstr.str(); 
        }
        
        int size()
        {
            typename vector<T>::const_iterator iter;
            int size = 0;
            for ( iter = this->chain.begin();
                    iter != this->chain.end();
                    iter++ )
            {
                size += iter->size();
            }
            return size;
        }

        off_t getValByPos( const int &pos );
};

// This is one request pattern, with offset and length
class RequestPattern {
    public:
        PatternElem offset;
        PatChain<PatternElem> length;

        vector<Request> futureRequests( int n, int startStride );
};

class RequestPatternList {
    public:
        vector<RequestPattern> list;
        
        const string show() const;
};




// This Tuple is a concept in the LZ77 compression algorithm.
// Please refer to http://jens.quicknote.de/comp/LZ77-JensMueller.pdf for details.
class Tuple {
    public:
        int offset; //note that this is not the 
        // offset when accessing file. But
        // the offset in LZ77 algorithm
        int length; //concept in LZ77
        off_t next_symbol;

        Tuple() {}
        Tuple(int o, int l, off_t n) {
            offset = o;
            length = l;
            next_symbol = n;
        }

        void put(int o, int l, off_t n) {
            offset = o;
            length = l;
            next_symbol = n;
        }

        bool operator== (const Tuple other) {
            if (offset == other.offset 
                    && length == other.length
                    && next_symbol == other.next_symbol)
            {
                return true;
            } else {
                return false;
            }
        }

        // Tell if the repeating sequences are next to each other
        bool isRepeatingNeighbor() {
            return (offset == length && offset > 0);
        }

        string show() {
            ostringstream showstr;
            showstr << "(" << offset 
                << ", " << length
                << ", " << next_symbol << ")" << endl;
            return showstr.str();
        }
};

// Get the pos th element in the pattern
// pos has to be in the range
template <class T>
inline
off_t
PatChain<T>::getValByPos( const int &pos ) 
{
    int cur_pos = 0; //it should always point to patunit.init

    typename vector<T>::iterator iter;
	
    for ( iter = this->chain.begin() ;
          iter != this->chain.end() ;
          iter++ )
    {
        int numoflen = iter->seq.size() * iter->cnt;
        
        if ( numoflen == 0 ) {
            numoflen = 1; //there's actually one element in *iter
        } 
        if (cur_pos <= pos && pos < cur_pos + numoflen ) {
            //in the range that pointed to by iter
            int rpos = pos - cur_pos;
            return iter->getValByPos( rpos );
        } else {
            //not in the range of iter
            cur_pos += numoflen; //keep track of current pos
        }
    }
    assert(0); // Make it hard for the errors
}



// pos has to be in the range
inline
off_t
PatternElem::getValByPos( const int &pos  ) 
{
    off_t locval = 0;
    int mpos;
    int col, row;
    off_t seqsum;
    off_t val = -1;

    if ( pos == 0 ) {
	    return init;
	}

    /* 
     * The caller should make sure this
     *
    if ( seq.size() == 0 || cnt == 0 || pos < 0 || pos >= seq.size()*cnt ) {
        // that's nothing in seq and you are requesting 
        // pos > 0. Sorry, no answer for that.
        ostringstream oss;
        oss << "In " << __FUNCTION__ << 
            " Request out of range. Pos is " << pos << endl;
        mlog (IDX_ERR, "%s", oss.str().c_str());
        assert(0); // Make it hard for the errors
    }
    */

    locval = init;
	mpos = pos - 1; //the position in the matrix
	col = mpos % seq.size();
    //cout << "col" << col << endl;
	row = mpos / seq.size();
    //cout << "row" << row << endl;

	if ( ! (row < cnt) ) {
        assert(0); // Make it hard for the errors
	}
	seqsum = sumVector(seq);
    //cout << "seqsum" << seqsum << endl;
	locval += seqsum * row;
	
	int i = 0;
	while ( i <= col ) {
		locval += seq[i];
        i++;
	}
    
    val = locval;
	return val;
}



// Each index has its own pattern
// This is the utility used to recognize patterns.
class PatternUtil {
    public:
        PatternUtil() {} 
        static void discoverPattern( vector<off_t> const &seq );
        static PatChain<PatternElem> discoverSigPattern( vector<off_t> const &seq,
                vector<off_t> const &orig );
        //It takes in a entry buffer like in PLFS,
        //analyzes it and generate Index Signature Entries
        //TODO: I'll come back for this function
        //IdxSigEntryList generatePatternUtil(vector<HostEntry> &entry_buf, int proc);
        static PatChain<PatternElem> findPattern( vector<off_t> deltas );
        static PatChain<PatternElem> getPatChainFromSeq( vector<off_t> inits );
        static int getReqPatList( const vector<Request> &reqs,
                                  RequestPatternList &patlist);
    private:
        static const int win_size = 6; //window size
        static Tuple searchNeighbor( vector<off_t> const &seq,
                vector<off_t>::const_iterator p_lookahead_win ); 
};

#endif

