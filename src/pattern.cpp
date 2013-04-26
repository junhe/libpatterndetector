#include "pattern.h"

#include <algorithm>

void
appendToBuffer( string &to, const void *from, const int size )
{
    if ( size > 0 ) { //make it safe
        to.append( (char *)from, size );
    }
}

//Note that this function will increase start
void
readFromBuf( string &from, void *to, int &start, const int size )
{
    //'to' has to be treated as plain memory
    memcpy(to, &from[start], size);
    start += size;
}

inline
off_t
sumVector( vector<off_t> seq )
{
    vector<off_t>::const_iterator iiter;
    
    off_t sum = 0;
    for ( iiter = seq.begin() ;
          iiter != seq.end() ;
          iiter++ )
    {
        sum += *iiter;
    }
    
    return sum;
}

string
printVector( vector<off_t> vec )
{
    ostringstream oss;
    oss << "PrintVector: " ;
    vector<off_t>::const_iterator it;
    for ( it =  vec.begin() ;
          it != vec.end() ;
          it++ )
    {
        oss << *it << "," ;
    }
    oss << endl;
    return oss.str();
}

// delta[i] = seq[i]-seq[i-1]
vector<off_t>
buildDeltas( vector<off_t> seq ) 
{
    vector<off_t>::iterator it;
    vector<off_t> deltas;
    //ostringstream oss;
    for ( it = seq.begin() ; it != seq.end() ; it++ )
    {
        if ( it > seq.begin() ) {
            deltas.push_back( *it - *(it-1) );
            //oss << (*it - *(it-1)) << "-";
        }
    }
    //mlog(IDX_WARN, "deltas:%s", oss.str().c_str());
    //cout << "in builddeltas: " << seq.size() << " " << deltas.size() << endl; 
    return deltas;
}


Request::Request(off_t off, off_t len)
    : offset(off), 
      length(len)
{}

Request::Request()
    : offset(0),
      length(0)
{}





///////////////////////////////////////////////
// PatternElem
///////////////////////////////////////////////

//return number of elements in total
int
PatternElem::size() const 
{
    if ( cnt == 0 ) {
        return 1; //not repetition
    } else {
        return seq.size()*cnt;
    }
}

string 
PatternElem::show() const
{
    vector<off_t>::const_iterator iter;
    ostringstream showstr;
    showstr << "( " ;
    for (iter = seq.begin();
            iter != seq.end();
            iter++ )
    {
        showstr << *iter << " ";
    }
    showstr << ") ^" << cnt << endl;
    return showstr.str();
}


//Serialiezd PatternUnit: [head:bodysize][body]
header_t
PatternUnit::bodySize()
{
    header_t totalsize;
    totalsize = sizeof(init) //init
                + sizeof(cnt) //cnt
                + sizeof(header_t) //length of seq size header
                + seq.size()*sizeof(off_t);
    return totalsize;
}

// check if this follows other and merge
// has to satisfy two:
// 1. seq are exactly the same OR (repeating and the same, size can be diff)
//    (3,3,3)==(3,3,3)             (3,3,3)==(3), (3,3,3)==()
// 2. AND init1 + sum of deltas == init2
// return true if appended successfully
bool
PatternUnit::append( PatternUnit &other )
{

//    mlog(IDX_WARN, "in %s", __FUNCTION__);

    if ( this->isSeqRepeating() 
        && other.isSeqRepeating() )
    {
        if ( this->size() > 1 && other.size() > 1 ) {
            //case 1. both has size > 1
            if ( this->seq[0] == other.seq[0] 
                 && this->init + this->seq[0]*this->size() == other.init ) {
                int newsize = this->size() + other.size();
                this->seq.clear();
                this->seq.push_back(other.seq[0]);
                this->cnt = newsize;
                return true;
            } else {
                return false;
            }               
        } else if ( this->size() == 1 && other.size() == 1 ) {
            //case 2. both has size == 1
            //definitely follows
            this->seq.clear();
            this->seq.push_back(other.init - this->init);
            this->cnt = 2; //has two now
            return true;
        } else if ( this->size() == 1 && other.size() > 1 ) {
            if ( other.init - this->init == other.seq[0] ) {
                int newsize = this->size() + other.size();
                this->seq.clear();
                this->seq.push_back(other.seq[0]);
                this->cnt = newsize;
                return true;
            } else {
                return false;
            }
        } else if ( this->size() > 1 && other.size() == 1) {
            if ( this->init + this->seq[0]*this->size() == other.init ) {
                int newsize = this->size() + other.size();
                off_t tmp = this->seq[0];
                this->seq.clear();
                this->seq.push_back(tmp);
                this->cnt = newsize;
                return true;
            } else {
                return false;
            }
        }
    } else {
        return false;  //TODO:should handle this case
    }
    // Never should reach here
    assert(0);
}

// (3,3,3)^4 is repeating
// (0,0)^0 is also repeating
bool
PatternUnit::isSeqRepeating()
{
    vector<off_t>::iterator it;
    bool allrepeat = true;
    for ( it = seq.begin();
          it != seq.end();
          it++ )
    {
        if ( it != seq.begin()
             && *it != *(it-1) ) {
            allrepeat = false;
            break;
        }
    }
    return allrepeat;
}

void
PatternUnit::compressRepeats()
{
    if ( isSeqRepeating() && size() > 1 ) {
        cnt = size();
        off_t tmp = seq[0];
        seq.clear();
        seq.push_back(tmp);
    }
}


string 
PatternUnit::serialize()
{
    string buf; //let me put it in string and see if it works
    header_t seqbodysize;
    header_t totalsize;

    totalsize = bodySize(); 
    
    appendToBuffer(buf, &totalsize, sizeof(totalsize));
    appendToBuffer(buf, &init, sizeof(init));
    appendToBuffer(buf, &cnt, sizeof(cnt));
    seqbodysize = seq.size()*sizeof(off_t);
    appendToBuffer(buf, &(seqbodysize), sizeof(header_t));
    if (seqbodysize > 0 ) {
        appendToBuffer(buf, &seq[0], seqbodysize);
    }
    return buf;
}

//This input buf should be [data size of the followed data][data]
void 
PatternUnit::deSerialize(string buf)
{
    header_t totalsize;
    int cur_start = 0;
    header_t seqbodysize;

    readFromBuf(buf, &totalsize, cur_start, sizeof(totalsize));
    readFromBuf(buf, &init, cur_start, sizeof(init));
    readFromBuf(buf, &cnt, cur_start, sizeof(cnt));
    readFromBuf(buf, &seqbodysize, cur_start, sizeof(header_t));
    if ( seqbodysize > 0 ) {
        seq.resize(seqbodysize/sizeof(off_t));
        readFromBuf(buf, &seq[0], cur_start, seqbodysize); 
    }
}


string
PatternUnit::show() const
{
    ostringstream showstr;
    showstr << init << " ... ";
    showstr << PatternElem::show();
    return showstr.str();
}

int
PatternUtil::getReqPatList( const vector<Request> &reqs,
                            RequestPatternList &patlist )
{
    vector<off_t> offsets; 
    vector<off_t> lengths;

    vector<Request>::const_iterator it;

    for ( it = reqs.begin() ; 
          it != reqs.end();
          it++ )
    {
        offsets.push_back(it->offset);
        lengths.push_back(it->length);
    }

    // Get the patterns of logical offsets first
    PatChain<PatternUnit>
        offset_pat = getPatChainFromSeq(offsets);
    
    //cout << "before show. offset.size()=" <<
    //     offsets.size() << endl;
    //cout << offset_pat.show() << endl;

    vector<PatternUnit>::const_iterator chain_iter;
    int range_start = 0, range_end; //the range currently processing
    for (chain_iter = offset_pat.begin();
            chain_iter != offset_pat.end();
            chain_iter++ ) 
    {
        RequestPattern reqpat;
        range_end = range_start
                  + max( int(chain_iter->cnt 
                             * chain_iter->seq.size()), 1 );  
        assert( ((ssize_t) range_end) <= offsets.size() );
        
        reqpat.offset = *chain_iter;
        vector<off_t> length_seq(lengths.begin()+range_start,
                                 lengths.begin()+range_end);
        reqpat.length = getPatChainFromSeq(length_seq);

        patlist.list.push_back( reqpat );
        
        range_start = range_end;
    }
    
    //cout << patlist.show();
    
    return EXIT_SUCCESS;
}

/* 
// It recognizes the patterns
// and returens a list of pattern entries.
IdxSigEntryList
PatternUtil::generatePatternUtil(
        vector<HostEntry> &entry_buf, 
        int proc) 
{
    vector<off_t> logical_offset, length, physical_offset; 
    vector<off_t> logical_offset_delta, 
                  length_delta, 
                  physical_offset_delta;
    IdxSigEntryList entrylist;
    vector<HostEntry>::const_iterator iter;

    // handle one process at a time.
    for ( iter = entry_buf.begin() ; 
            iter != entry_buf.end() ;
            iter++ )
    {
        if ( iter->id != proc ) {
            continue;
        }

        logical_offset.push_back(iter->logical_offset);
        length.push_back(iter->length);
        physical_offset.push_back(iter->physical_offset);
    }
   
    // only for debuggin. 
    if ( !(logical_offset.size() == length.size() &&
            length.size() == physical_offset.size()) ) {
        ostringstream oss;
        oss << "logical_offset.size():" << logical_offset.size() 
            << "length.size():" << length.size()
            << "physical_offset.size():" << physical_offset.size() << endl;
        mlog(IDX_WARN, "sizes should be equal. %s", oss.str().c_str());
        exit(-1);
    }


    // Get the patterns of logical offsets first
    PatChain<PatternUnit> offset_sig = getPatChainFromSeq(logical_offset);

    //Now, go through offset_sig one by one and build the IdxSigEntry s
    vector<IdxSigEntry>idx_entry_list;
    vector<PatternUnit>::const_iterator stack_iter;

    int range_start = 0, range_end; //the range currently processing
    for (stack_iter = offset_sig.begin();
            stack_iter != offset_sig.end();
            stack_iter++ )
    {
        //cout << stack_iter->init << " " ;
        IdxSigEntry idx_entry;
        range_end = range_start 
                    + max( int(stack_iter->cnt * stack_iter->seq.size()), 1 );  
//        mlog(IDX_WARN, "range_start:%d, range_end:%d, logical_offset.size():%d",
//                range_start, range_end, logical_offset.size());
        assert( range_end <= logical_offset.size() );

        idx_entry.original_chunk = proc;
        idx_entry.logical_offset = *stack_iter;
       
        // Find the corresponding patterns of lengths
        vector<off_t> length_seg(length.begin()+range_start,
                                 length.begin()+range_end);
        idx_entry.length = getPatChainFromSeq(length_seg);

        // Find the corresponding patterns of physical offsets
        vector<off_t> physical_offset_seg( physical_offset.begin()+range_start,
                                           physical_offset.begin()+range_end);
        idx_entry.physical_offset = getPatChainFromSeq(physical_offset_seg);
        
        idx_entry_list.push_back( idx_entry);

        range_start = range_end;
    }
    entrylist.append(idx_entry_list);
    //printIdxEntries(idx_entry_list);
    return entrylist;
}

*/


// This is the key function of LZ77. 
// The input is a sequence of number and 
// it returns patterns like: i,(x,x,x,..)^r, ...
PatChain<PatternUnit>
PatternUtil::getPatChainFromSeq( vector<off_t> inits )
{
    //mlog(IDX_WARN, "Entering %s. inits: %s", 
    //     __FUNCTION__, printVector(inits).c_str());
    vector<off_t> deltas = buildDeltas(inits);
    PatChain<PatternUnit> pattern, patterntrim;

    pattern = findPattern( deltas ); // find repeating pattern in deltas

    // Put the init back into the patterns
    int pos = 0;
    vector<PatternUnit>::iterator it;
    for ( it = pattern.chain.begin() ;
          it != pattern.chain.end() ;
          it++ )
    {
        it->init = inits[pos];
        pos += it->seq.size() * it->cnt;
    }


    // handle the missing last
    if ( ! inits.empty() ) {
        if ( ! pattern.chain.empty() 
             && pattern.chain.back().seq.size() == 1 
             && pattern.chain.back().init + 
                pattern.chain.back().seq[0] * pattern.chain.back().cnt 
                 == inits.back() ) 
        {
            pattern.chain.back().cnt++;
        } else {        
            PatternUnit punit;
            punit.init = inits.back();
            punit.cnt = 1;
            pattern.push(punit);
        }
    }

    // combine the unecessary single patterns
    for ( it = pattern.chain.begin() ;
          it != pattern.chain.end() ;
          it++ )
    {
        ostringstream oss;
        oss << it->show() ;
//        mlog(IDX_WARN, "TRIM:%s", oss.str().c_str());
        
        if ( it != pattern.chain.begin()
             && it->cnt * it->seq.size() <= 1
             && patterntrim.chain.back().seq.size() == 1
             && patterntrim.chain.back().cnt > 1
             && patterntrim.chain.back().init
                + patterntrim.chain.back().seq[0]
                * patterntrim.chain.back().cnt == it->init
             ) 
        {
            // it can be represented by the last pattern.
//            mlog(IDX_WARN, "in back++");
            patterntrim.chain.back().cnt++;
        } else {
//            mlog(IDX_WARN, "in push");
            patterntrim.push(*it);
        }
    }
   
    
    /*
    ostringstream oss;
    oss << pattern.show() << endl; 
    mlog(IDX_WARN, "Leaving %s:{%s}", __FUNCTION__, oss.str().c_str());
    */

    return patterntrim;
}

// It returns repeating patterns of a sequence of numbers. (x,x,x...)^r
PatChain<PatternUnit> 
PatternUtil::findPattern( vector<off_t> deltas )
{
    // pointer(iterator) to the lookahead window, both should move together
//    mlog(IDX_WARN, "Entering %s", __FUNCTION__);
//    mlog(IDX_WARN, "deltas: %s", printVector(deltas).c_str());
    vector<off_t>::const_iterator p_lookahead_win;
    PatChain<PatternUnit> pattern_stack;

    p_lookahead_win = deltas.begin();
    pattern_stack.clear();

    while ( p_lookahead_win < deltas.end() ) {
        //lookahead window is not empty
//        mlog(IDX_WARN, "window position:%d", p_lookahead_win - deltas.begin());
        Tuple cur_tuple = searchNeighbor( deltas, p_lookahead_win );
        if ( cur_tuple.isRepeatingNeighbor() ) {
            if ( pattern_stack.isPopSafe( cur_tuple.length ) ) {
//                mlog(IDX_WARN, "SAFE" );
                //safe
                //pop out elements without breaking existing patterns
                pattern_stack.popElem( cur_tuple.length );

                vector<off_t>::const_iterator first, last;
                first = p_lookahead_win;
                last = p_lookahead_win + cur_tuple.length;

                PatternUnit pu;
                pu.seq.assign(first, last);
                pu.cnt = 2;

                pattern_stack.push( pu );
                p_lookahead_win += cur_tuple.length;
            } else {
                //unsafe
//                mlog(IDX_WARN, "UNSAFE" );
                PatternUnit pu = pattern_stack.top();

                if ( cur_tuple.length % pu.seq.size() == 0
                     && cur_tuple.length <= pu.seq.size() * pu.cnt ) {
//                    mlog(IDX_WARN, "-REPEATING LAST Pattern");
                    //the subseq in lookahead window repeats
                    //the top pattern in stack.
                    //initial remains the same.
                    pu.cnt += cur_tuple.length / pu.seq.size() ;
                    pattern_stack.popPattern();
                    pattern_stack.push(pu);

                    p_lookahead_win += cur_tuple.length;
                } else {
//                    mlog(IDX_WARN, "-Just Pust it in");
                    //cannot pop out cur_tuple.length elems without
                    //totally breaking any pattern.
                    //So we simply add one elem to the stack
                    PatternUnit pu;
                    pu.seq.push_back( *p_lookahead_win );
                    pu.cnt = 1;
                    pattern_stack.push(pu);
                    p_lookahead_win++;
                }
            }
        } else {
            //(0,0,x), nothing repeats
            PatternUnit pu;
            pu.seq.push_back(cur_tuple.next_symbol);
            pu.cnt = 1;

            pattern_stack.push(pu); 
            p_lookahead_win++;
        }
        pattern_stack.chain.back().compressRepeats();
//        mlog(IDX_WARN, "LOOP: %s", pattern_stack.show().c_str());
    }
   
//    mlog(IDX_WARN, "Leaving %s:%s", __FUNCTION__, pattern_stack.show().c_str());
    return pattern_stack;

}

Tuple 
PatternUtil::searchNeighbor( vector<off_t> const &seq,
        vector<off_t>::const_iterator p_lookahead_win ) 
{
    vector<off_t>::const_iterator i;     
    int j;
    //cout << "------------------- I am in searchNeighbor() " << endl;

    //i goes left util the begin or reaching window size
    i = p_lookahead_win;
    int remain = seq.end() - p_lookahead_win;
    while ( i != seq.begin() 
            && (p_lookahead_win - i) < win_size
            && (p_lookahead_win - i) < remain ) {
        i--;
    }
    //termination: i == seq.begin() or distance == win_size

    /*
    //print out search buffer and lookahead buffer
    //cout << "search buf: " ;
    vector<off_t>::const_iterator k;
    for ( k = i ; k != p_lookahead_win ; k++ ) {
    cout << *k << " ";
    }
    cout << endl;

    cout << "lookahead buf: " ;
    vector<off_t>::const_iterator p;
    p = p_lookahead_win;
    for ( p = p_lookahead_win ; 
    p != seq.end() && p - p_lookahead_win < win_size ; 
    p++ ) {
    cout << *p << " ";
    }
    cout << endl;
    */

    //i points to a element in search buffer where matching starts
    //j is the iterator from the start to the end of search buffer to compare
    //smarter algorithm can be used better time complexity, like KMP.
    for ( ; i != p_lookahead_win ; i++ ) {
        int search_length = p_lookahead_win - i;
        for ( j = 0 ; j < search_length ; j++ ) {
            if ( *(i+j) != *(p_lookahead_win + j) ) {
                break;
            }
        }
        if ( j == search_length ) {
            //found a repeating neighbor
            return Tuple(search_length, search_length, 
                    *(p_lookahead_win + search_length));
        }
    }

    //Cannot find a repeating neighbor
    return Tuple(0, 0, *(p_lookahead_win));
}




#if 0







string 
IdxSigEntry::show()
{
    ostringstream showstr;

    showstr << "[" << original_chunk << "]" 
         << "[" << new_chunk_id << "]" << endl;
    showstr << "----Logical Offset----" << endl;
    showstr << logical_offset.show();
    
    vector<PatternUnit>::const_iterator iter2;

    showstr << "----Length----" << endl;
    for (iter2 = length.begin();
            iter2 != length.end();
            iter2++ )
    {
        showstr << iter2->show(); 
    }

    showstr << "----Physical Offset----" << endl;
    for (iter2 = physical_offset.begin();
            iter2 != physical_offset.end();
            iter2++ )
    {
        showstr << iter2->show(); 
    }
    showstr << "-------------------------------------" << endl;

    return showstr.str();
}

#endif

const string
RequestPatternList::show() const
{
    vector<RequestPattern>::const_iterator it;
    vector<PatternUnit>::const_iterator it2;
    
    ostringstream oss;

    for ( it = list.begin() ;
          it != list.end() ;
          it++ )
    {
        oss << "--- offset ---" << endl;
        oss << it->offset.show();
        for ( it2 = it->length.chain.begin() ; 
              it2 != it->length.chain.end() ;
              it2++ )
        {
            oss << "--- length ---" << endl;
            oss << it2->show() << endl;
        }
    }
    return oss.str();
}

// return the future n requests following
// this pattern
//
// the input n is the number of requests you want to predict
// startStride is how many stride this future request should start from.
vector<Request>
RequestPattern::futureRequests( int n , int startStride )
{
    // let me handle the simple case first
    // one offset pattern and one length pattern
    assert( length.chain.size() == 1 && length.chain[0].cnt > 0 );

    vector<Request> freq; // future requests put here

    // handle the case of only one request in the pattern
    if ( offset.seq.size() == 0
            || ( offset.seq.size() * offset.cnt <= 1 ) )
    {
        assert(length.chain.size() > 0);
        freq.push_back( Request(offset.init + length.chain[0].init,
                                length.chain[0].init * n) );
        return freq;
    }

    off_t off_stride_sum = sumVector(offset.seq);
    PatternUnit mylen = length.chain[0]; // for short
    off_t len_stride_sum = sumVector(mylen.seq);
    int i;
    int offseqsize = offset.seq.size();
    int lenseqsize = mylen.seq.size();


    int round = ((startStride-1) * n) / offseqsize;
    int remain = ((startStride-1) * n) % offseqsize;

    off_t off_add = off_stride_sum * round;
    off_t len_add = len_stride_sum * round;
    for ( i = 0 ; i < remain ; i++ ) {
        off_add += offset.seq[ i % offseqsize ];
        len_add += mylen.seq[ i% lenseqsize ];
    }

    off_t base_off = offset.init + off_stride_sum * offset.cnt + off_add;
    off_t base_len = mylen.init + len_stride_sum * mylen.cnt + len_add;
    
    freq.push_back( Request(base_off, base_len) );


    if ( offseqsize == 0 || lenseqsize == 0 )
        return freq;

    off_t curoff = base_off;
    off_t curlen = base_len;
    for ( i = 0 ; i < n-1 ; i++ ) {
        curoff = curoff + offset.seq[ i % offseqsize ];
        curlen = curlen + mylen.seq[ i % lenseqsize ];
        freq.push_back( Request(curoff, curlen) );
    }
    return freq;
}


