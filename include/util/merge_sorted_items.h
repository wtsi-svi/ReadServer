#ifndef _MERGE_SORTED_ITEMS_H
#define _MERGE_SORTED_ITEMS_H

#include <string>
#include <fstream>
#include <map>
#include <iostream>
#include <stdlib.h>

typedef struct {
  std::string sequence;
  std::string ids;
} ReadInfo;

enum class CompareResult {
  Equal = 0,
  Smaller = 1,
  Greater = 2
};

template <typename ReadStrategy>
class ReadInfoReader : private ReadStrategy {
 public:
  ReadInfoReader () {
    m_hasNext = false;
  }

  ~ReadInfoReader () { m_ifs.close(); }

  ReadInfoReader ( const ReadInfoReader& scr ) = delete;
  ReadInfoReader& operator= ( const ReadInfoReader& src ) = delete;

  void open ( const std::string& filename ) {
    m_hasNext = false;
    m_ifs.open(filename.c_str(), std::ios::in);
    if ( m_ifs ) {
      readNext(CompareResult::Smaller);
    }
  }

  void close () {
    m_ifs.close();
  }

  bool hasNext () const {
    return m_hasNext;
  }

  void readNext ( const CompareResult& result = CompareResult::Smaller ) {
    if ( result < CompareResult::Greater ) {
      m_next = ReadInfo();
      m_hasNext = ReadStrategy::read(m_ifs, m_next);
    }
  }

  ReadInfo getNext () const {
    return m_next;
  }

 private:
  std::ifstream m_ifs;
  ReadInfo m_next;
  bool m_hasNext;
};

template <typename WriteStrategy>
class ReadInfoWriter : private WriteStrategy {
 public:
  ReadInfoWriter () {}

  ~ReadInfoWriter () { m_ofs.close(); }

  ReadInfoWriter ( const ReadInfoWriter& scr ) = delete;
  ReadInfoWriter& operator= ( const ReadInfoWriter& src ) = delete;

  void open ( const std::string& filename ) {
    m_ofs.open(filename.c_str(), std::ios::out);
  }

  void write ( const ReadInfo& ri ) {
    m_ofs << WriteStrategy::write(ri);
  }

  void write ( const CompareResult& result, const ReadInfo& ri1, const ReadInfo& ri2 ) {
    switch ( result ) {
    case CompareResult::Smaller:
      m_ofs << WriteStrategy::write(ri1);
      break;
    case CompareResult::Greater:
      m_ofs << WriteStrategy::write(ri2);
      break;
    default:
      m_ofs << WriteStrategy::write(ri1, ri2);
      break;
    }
  }

 private:
  std::ofstream m_ofs;
};

template <typename Reader, typename Writer, typename Comparator>
  class MergeSortedItems : private Reader, private Writer, private Comparator {
 public:
  MergeSortedItems ( const std::string& file1, const std::string& file2, const std::string& out ) {
    m_rd1.open(file1);
    m_rd2.open(file2);
    m_wr.open(out);
  }
     

  ~MergeSortedItems () {}

  void merge () {
    Comparator comparator;
    while ( m_rd1.hasNext() || m_rd2.hasNext() ) {
      if ( !m_rd1.hasNext() ) { // file1 has reached eof
        const ReadInfo& ri2 = m_rd2.getNext();
        m_wr.write(ri2);
        m_rd2.readNext(CompareResult::Smaller);
        continue;
      }

      if ( !m_rd2.hasNext() ) { // file2 has reached eof
        const ReadInfo& ri1 = m_rd1.getNext();
        m_wr.write(ri1);
        m_rd1.readNext(CompareResult::Smaller);
        continue;
      }

      // both has item.
      const ReadInfo& ri1 = m_rd1.getNext();
      const ReadInfo& ri2 = m_rd2.getNext();
      const CompareResult result = comparator(ri1, ri2);
      m_wr.write(result, ri1, ri2);
      m_rd1.readNext(result);
      m_rd2.readNext(result==CompareResult::Equal ? CompareResult::Equal : (result==CompareResult::Smaller ? CompareResult::Greater : CompareResult::Smaller));
    }
  }

 private:
  Reader m_rd1;
  Reader m_rd2;
  Writer m_wr;
};

#endif
