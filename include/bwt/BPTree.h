#ifndef BPTREE_H
#define BPTREE_H

#include <vector>
#include <memory>

#include "BPNodes.h"

#define DEFAULT_SYMBOL_RATE 1024 * 64 // strictly power of 2.
#define DEFAULT_SYMBOL_SHIFT_VALUE 16 // 2 ** 15 = 1024 * 32
#define DEFAULT_LARGE_SHIFT_VALUE 10 // 
#define DEFAULT_SMALL_SHIFT_VALUE 4 // 

struct Marker {
  size_t index;
  size_t sum;
  size_t count;
};

class BPTree {
 public:
  
  BPTree () {}

  void insertAtFront ( const std::shared_ptr<IBPNodes>& nodes ) {
    m_vNodes.insert(m_vNodes.begin(), nodes);
  }

  void push_back ( const std::shared_ptr<IBPNodes>& nodes ) {
    m_vNodes.push_back(nodes);
  }

  void setSumVector ( const std::shared_ptr< std::vector<uint32_t> >& rhs ) {
    m_vSum = rhs;
  }

  inline std::shared_ptr<IBPNodes> at ( const size_t& index ) const {
    return m_vNodes[index];
  }

  inline std::shared_ptr<std::vector<uint32_t> > getSumVector () const {
    return m_vSum;
  }

  inline size_t depth () const {
    return m_vNodes.size();
  }

  // return the floor marker at which base count (bc) for char b is less than but not equal to bc. 
  inline Marker select ( const char& b, const size_t& bc ) const {
    size_t offset = 0;
    size_t count = 0;
    size_t sum = 0;

    for ( std::vector< std::shared_ptr<IBPNodes> >::const_iterator it=m_vNodes.begin(); it!=m_vNodes.end(); ++it ) {
      offset = (*it)->select(b, bc-count, offset);
      sum += (*it)->sum(offset);
      count += (*it)->count(b, offset);
    }
    
    Marker mk;
    mk.index = offset;
    mk.sum = sum;
    mk.count = count;

    return mk;
  }

  inline Marker rank ( const char& b, const size_t& index ) const {
    size_t offset = 0;
    size_t count = 0;
    size_t sum = 0;
    
    size_t lb = index >> DEFAULT_SYMBOL_SHIFT_VALUE; // lower bound
    const size_t idxAtSum = (*m_vSum)[lb];
    size_t nextAtSum = idxAtSum;
    ++lb;
    if ( lb < m_vSum->size()) {
      nextAtSum = (*m_vSum)[lb];
    }

    const size_t td = depth();

    if ( idxAtSum == nextAtSum ) {
      offset = idxAtSum >> ( DEFAULT_SMALL_SHIFT_VALUE * (td>1 ? 1 : 0) + DEFAULT_LARGE_SHIFT_VALUE * (td>2 ? td-2 : 0) );
      sum += m_vNodes[0]->sum(offset);
      count += m_vNodes[0]->count(b, offset);

      for ( size_t i=1; i<td; ++i ) {
        offset = m_vNodes[i]->access(index-sum, offset);
        sum += m_vNodes[i]->sum(offset);
        count += m_vNodes[i]->count(b, offset);
      }
    }
    else {
      for ( size_t i=0; i<td; ++i ) {
        const size_t sv = DEFAULT_SMALL_SHIFT_VALUE * (td-i>1 ? 1 : 0) + DEFAULT_LARGE_SHIFT_VALUE * (td-i>2 ? td-i-2 : 0);
        size_t lower = idxAtSum >> sv;
        size_t upper = nextAtSum >> sv;

        const size_t bs = m_vNodes[i]->getBlockSize();
        const size_t lower_bound = offset * bs;
        const size_t upper_bound = lower_bound + bs - 1;

        if ( lower_bound > lower ) {
          lower = lower_bound;
        }
        if ( upper_bound < upper ) {
          upper =  upper_bound;
        }

        offset = m_vNodes[i]->access(index-sum, lower, upper);
        sum += m_vNodes[i]->sum(offset);
        count += m_vNodes[i]->count(b, offset);
      }
    }


    if ( offset == 0 ) {
      offset = idxAtSum;
    }

    Marker mk;
    mk.index = offset;
    mk.sum = sum;
    mk.count = count;

    return mk;
  }

  inline Marker access ( const size_t& index ) const {
    size_t offset = 0;
    size_t sum = 0;
    
    size_t lb = index >> DEFAULT_SYMBOL_SHIFT_VALUE; // lower bound
    const size_t idxAtSum = (*m_vSum)[lb];
    size_t nextAtSum = idxAtSum;
    ++lb;
    if ( lb < m_vSum->size()) {
      nextAtSum = (*m_vSum)[lb];
    }

    const size_t td = depth();

    for ( size_t i=0; i<td; ++i ) {
      IBPNodes* nodes = m_vNodes[i].get();
      const size_t sv = DEFAULT_SMALL_SHIFT_VALUE * (td-i>1 ? 1 : 0) + DEFAULT_LARGE_SHIFT_VALUE * (td-i>2 ? td-i-2 : 0);
      size_t lower = idxAtSum >> sv;

      if ( idxAtSum == nextAtSum ) {
        if ( offset == 0 ) {
          offset = lower;
        }
        else {
          offset = nodes->access(index-sum, offset);
        }
      }
      else {
        size_t upper = nextAtSum >> sv;
        const size_t bs = nodes->getBlockSize();
        const size_t lower_bound = offset * bs;
        const size_t upper_bound = lower_bound + bs - 1;

        if ( lower_bound > lower ) {
          lower = lower_bound;
        }
        if ( upper_bound < upper ) {
          upper =  upper_bound;
        }

        offset = nodes->access(index-sum, lower, upper);
      }

      sum += nodes->sum(offset);
    }

    if ( offset == 0 ) {
      offset = idxAtSum;
    }

    Marker mk;
    mk.index = offset;
    mk.sum = sum;
    mk.count = sum;

    return mk;
  }

  void serialise ( std::ostream& os ) const {
    for ( std::vector< std::shared_ptr<IBPNodes> >::const_iterator it=m_vNodes.begin(); it!=m_vNodes.end(); ++it ) {
      (*it)->serialise(os);
    }

    const size_t sz = m_vSum->size();
    os.write((char*)&sz, sizeof(sz));
    for ( std::vector<uint32_t>::const_iterator it=m_vSum->begin(); it!=m_vSum->end(); ++it ) {
      os.write((char*)&(*it), sizeof(uint32_t));
    }
  } 

 private:

  std::vector< std::shared_ptr<IBPNodes> > m_vNodes;

  std::shared_ptr< std::vector<uint32_t> > m_vSum;
};

#endif
