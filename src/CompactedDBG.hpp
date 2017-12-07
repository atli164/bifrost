#ifndef COMPACTED_DBG_HPP
#define COMPACTED_DBG_HPP

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <getopt.h>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <thread>
#include <atomic>

/*
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <future>
#include <stdexcept>
*/

#include "BlockedBloomFilter.hpp"
#include "Common.hpp"
#include "fastq.hpp"
#include "Kmer.hpp"
#include "KmerHashTable.h"
#include "KmerIterator.hpp"
#include "minHashIterator.hpp"
#include "RepHash.hpp"
#include "TinyVector.hpp"
#include "Unitig.hpp"
#include "UnitigIterator.hpp"
#include "UnitigMap.hpp"

#define MASK_CONTIG_ID (0xffffffff00000000)
#define MASK_CONTIG_TYPE (0x80000000)
#define MASK_CONTIG_POS (0x7fffffff)
#define RESERVED_ID (0xffffffff)

#define DEFAULT_K 31
#define DEFAULT_G 23

/** @file src/CompactedDBG.hpp
* Interface for the Compacted de Bruijn graph API.
* Code snippets using this interface are provided in snippets.hpp.
*/

using namespace std;

/** @struct CDBG_Build_opt
* @brief Each member of this structure is a parameter for the function CompactedDBG<T>::build.
* Members CDBG_Build_opt::k and CDBG_Build_opt::g are not used by the function CompactedDBG<T>::build
* as they must be set in the constructor of the graph. Members CDBG_Build_opt::clipTips and
* CDBG_Build_opt::deleteIsolated are not used by the function CompactedDBG<T>::build as they are
* parameters of function CompactedDBG<T>::simplify. Members CDBG_Build_opt::prefixFilenameGFA and
* CDBG_Build_opt::filenameGFA are not used by CompactedDBG<T>::build but by CompactedDBG<T>::write.
* An example of using such a structure is shown in src/Bifrost.cpp.
* @var CDBG_Build_opt::reference_mode
* Reference mode, input are assembled genomes (no filtering step) if true, reads otherwise.
* @var CDBG_Build_opt::verbose
* Print information messages during execution if true.
* @var CDBG_Build_opt::clipTips
* Clip short (length < 2k) tips of the graph (not used by CompactedDBG<T>::build).
* @var CDBG_Build_opt::deleteIsolated
* Remove short (length < 2k) isolated unitigs of the graph (not used by CompactedDBG<T>::build).
* @var CDBG_Build_opt::k
* Length of k-mers (not used by CompactedDBG<T>::build).
* @var CDBG_Build_opt::g
* Length of g-mers, the minimizers, such that g < k (not used by CompactedDBG<T>::build).
* @var CDBG_Build_opt::nb_threads
* Number of threads to use for building the graph.
* @var CDBG_Build_opt::read_chunksize
* Number of reads shared and processed by CDBG_Build_opt::nb_threads threads at the same time.
* @var CDBG_Build_opt::unitig_size
* Maximum length of a unitig.
* @var CDBG_Build_opt::nb_unique_kmers
* Estimated number (upper bound) of different k-mers in the input FASTA/FASTQ files of
* CDBG_Build_opt::fastx_filename_in.
* @var CDBG_Build_opt::nb_non_unique_kmers
* Estimated number (upper bound) of different k-mers occurring twice or more in the FASTA/FASTQ files
* of CDBG_Build_opt::fastx_filename_in.
* @var CDBG_Build_opt::nb_bits_unique_kmers_bf
* Number of Bloom filter bits per k-mer occurring at least once in the FASTA/FASTQ files of
* CDBG_Build_opt::fastx_filename_in.
* @var CDBG_Build_opt::nb_bits_non_unique_kmers_bf
* Number of Bloom filter bits per k-mer occurring at least twice in the FASTA/FASTQ files of
* CDBG_Build_opt::fastx_filename_in.
* @var CDBG_Build_opt::prefixFilenameGFA
* Name of the file to which the graph must be written (not used by CompactedDBG<T>::build).
* @var CDBG_Build_opt::filenameGFA
* Name of the file to which the graph must be written + extension ".gfa". Set by CompactedDBG<T>::write
* (not used by CompactedDBG<T>::build).
* @var CDBG_Build_opt::inFilenameBBF
* String containing the name of a Bloom filter file that was generated by CompactedDBG<T>::filter.
* If empty, the filtering step is executed. Otherwise, the Bloom filter is loaded from this file
* and the filtering step is skipped.
* @var CDBG_Build_opt::outFilenameBBF
* String containing the name of a Bloom filter file that will be generated by CompactedDBG<T>::filter.
* If empty, the file is not created. Otherwise, the Bloom filter is written to this file.
* @var CDBG_Build_opt::fastx_filename_in
* vector of strings, each string is the name of a FASTA/FASTQ file to use for the graph construction.
*/
struct CDBG_Build_opt {

    bool reference_mode;
    bool verbose;

    size_t nb_threads;
    size_t read_chunksize;
    size_t unitig_size;
    size_t nb_unique_kmers;
    size_t nb_non_unique_kmers;
    size_t nb_bits_unique_kmers_bf;
    size_t nb_bits_non_unique_kmers_bf;

    string inFilenameBBF;
    string outFilenameBBF;

    vector<string> fastx_filename_in;

    // The following members are not used by CompactedDBG<T>::build
    // but you can set them to use them as parameters for other functions
    // such as CompactedDBG<T>::simplify or CompactedDBG<T>::write.

    size_t k, g;
    bool clipTips;
    bool deleteIsolated;

    string prefixFilenameGFA;
    string filenameGFA;

    CDBG_Build_opt() :  nb_threads(1), k(DEFAULT_K), g(DEFAULT_G), nb_unique_kmers(0), nb_non_unique_kmers(0), nb_bits_unique_kmers_bf(14),
                        nb_bits_non_unique_kmers_bf(14), read_chunksize(10000), unitig_size(1000000), reference_mode(false),
                        verbose(false), clipTips(false), deleteIsolated(false), inFilenameBBF(""), outFilenameBBF("") {}
};

/** @class CDBG_Data_t
* @brief If data are to be associated with the unitigs of the compacted de Bruijn graph, these data
* must be wrapped into a class that inherits from the abstract class CDBG_Data_t. Otherwise it
* will not compile. Hence, to associate data of type myData to unitigs, class myData must be declared as follows:
* \code{.cpp}
* class myData : public CDBG_Data_t<myData> { ... };
* \endcode
* Because CDBG_Data_t is an abstract class and its functions are virtual, CDBG_Data_t<T>::join and CDBG_Data_t<T>::split
* must be implemented in your wrapper. An example of using such a structure is shown in src/snippets.cpp.
*/
template<typename T> //Curiously Recurring Template Pattern (CRTP)
class CDBG_Data_t {

    public:

        /** Merge current object with another object of the same type (because the unitigs they are associated with
        * are going to be merged).
        * @param data is the object to merge with.
        * @param cdbg is the compacted de Bruijn graph from which the objects are from.
        */
        virtual void join(const T& data, CompactedDBG<T>& cdbg) = 0;
        /** Create from the current object, associated with unitig seq_un, a new object to associate with a new
        * unitig seq_un' = seq_un[pos, pos+len]
        * @param pos corresponds to the start position of the sub-unitig seq_un' into unitig seq_un.
        * @param len corresponds to the length of the sub-unitig seq_un'.
        * @param new_data is an new (empty) object that you can fill in with new data.
        * @param cdbg is the compacted de Bruijn graph from which the current object is from.
        */
        virtual void split(const size_t pos, const size_t len, T& new_data, CompactedDBG<T>& cdbg) const;
};

/** @class CompactedDBG
* @brief Represent a Compacted de Bruijn graph. The template parameter of this class corresponds to the type of data
* to associate with the unitigs of the graph. If no template parameter is specified or if the type is void, such as in
* \code{.cpp}
* CompactedDBG<> cdbg_1;
* CompactedDBG<void> cdbg_2; //Equivalent to previous notation
* \endcode
* then, no data are associated with the unitigs and no memory will be allocated for it. If data are to be associated
* with the unitigs, these data must be wrapped into a class that inherits from the abstract class CDBG_Data_t, such as in:
* \code{.cpp}
* class myData : public CDBG_Data_t<myData> { ... };
* CompactedDBG<myData> cdbg;
* \endcode
*/
template<typename T = void>
class CompactedDBG {

    static_assert(is_base_of<CDBG_Data_t<T>, T>::value || is_void<T>::value,
                  "Type of data associated with vertices of class CompactedDBG must be void (no data) or a class extending class CDBG_Data_t");

    private:

        int k_;
        int g_;

        bool invalid;

        const bool has_data;

        static const int tiny_vector_sz = 2;
        static const int min_abundance_lim = 15;
        static const int max_abundance_lim = 15;

        typedef KmerHashTable<CompressedCoverage_t<T>> h_kmers_ccov_t;
        typedef MinimizerHashTable<tiny_vector<size_t,tiny_vector_sz>> hmap_min_unitigs_t;

        typedef typename hmap_min_unitigs_t::iterator hmap_min_unitigs_iterator;
        typedef typename hmap_min_unitigs_t::const_iterator hmap_min_unitigs_const_iterator;

        vector<Unitig<T>*> v_unitigs;
        vector<pair<Kmer, CompressedCoverage_t<T>>> v_kmers;

        hmap_min_unitigs_t hmap_min_unitigs;
        h_kmers_ccov_t h_kmers_ccov;

        BlockedBloomFilter bf;

    public:

        typedef T U;

        template<typename U> friend class UnitigMap;
        template<typename U, bool is_const> friend class unitigIterator;
        template<typename U, bool is_const> friend class neighborIterator;

        typedef unitigIterator<T, false> iterator; /**< An iterator for the unitigs of the graph. No specific order is assumed. */
        typedef unitigIterator<T, true> const_iterator; /**< A constant iterator for the unitigs of the graph. No specific order is assumed. */

        CompactedDBG(int kmer_length = DEFAULT_K, int minimizer_length = DEFAULT_G);
        ~CompactedDBG();

        void clear();
        void empty();

        /** Return the length of k-mers of the graph.
        * @return Length of k-mers of the graph.
        */
        inline int getK() const { return k_; }

        /** Return the number of unitigs in the graph.
        * @return Number of unitigs in the graph.
        */
        inline size_t size() const { return v_unitigs.size() + v_kmers.size() + h_kmers_ccov.size(); }

        bool build(const CDBG_Build_opt& opt);
        bool simplify(const bool delete_short_isolated_unitigs = true, const bool clip_short_tips = true, const bool verbose = false);
        bool write(const string output_filename, const size_t nb_threads = 1, const bool verbose = false);

        UnitigMap<T> find(const Kmer& km, const bool extremities_only = false);

        bool add(const string& seq, const bool verbose = false);
        bool remove(const UnitigMap<T>& um, const bool verbose = false);

        iterator begin();
        const_iterator begin() const;

        iterator end();
        const_iterator end() const;

    private:

        bool join(const UnitigMap<T>& um, const bool verbose);

        bool join(const bool verbose);

        bool filter(const CDBG_Build_opt& opt);
        bool construct(const CDBG_Build_opt& opt);

        bool addUnitigSequenceBBF(Kmer km, const string& read, size_t pos, const string& seq, vector<Kmer>& l_ignored_km_tip);
        size_t findUnitigSequenceBBF(Kmer km, string& s, bool& selfLoop, bool& isIsolated, vector<Kmer>& l_ignored_km_tip);
        bool bwStepBBF(Kmer km, Kmer& front, char& c, bool& has_no_neighbor, vector<Kmer>& l_ignored_km_tip, bool check_fp_cand = true) const;
        bool fwStepBBF(Kmer km, Kmer& end, char& c, bool& has_no_neighbor, vector<Kmer>& l_ignored_km_tip, bool check_fp_cand = true) const;

        UnitigMap<T> findUnitig(const Kmer& km, const string& s, size_t pos);
        UnitigMap<T> findUnitig(const Kmer& km, const string& s, size_t pos, const preAllocMinHashIterator<RepHash>& it_min_h);

        bool addUnitig(const string& str_unitig, const size_t id_unitig);
        void deleteUnitig(const bool isShort, const bool isAbundant, const size_t id_unitig);
        void swapUnitigs(const bool isShort, const size_t id_a, const size_t id_b);
        bool splitUnitig(size_t& pos_v_unitigs, size_t& nxt_pos_insert_v_unitigs, size_t& v_unitigs_sz, size_t& v_kmers_sz,
                        const vector<pair<int,int>>& sp);

        UnitigMap<T> find(const Kmer& km, const preAllocMinHashIterator<RepHash>& it_min_h);
        vector<UnitigMap<T>> findPredecessors(const Kmer& km, const bool extremities_only = false);
        vector<UnitigMap<T>> findSuccessors(const Kmer& km, const size_t limit = 4, const bool extremities_only = false);

        inline size_t find(const preAllocMinHashIterator<RepHash>& it_min_h) const {

            const int pos = it_min_h.getPosition();
            return (hmap_min_unitigs.find(Minimizer(&it_min_h.s[pos]).rep()) != hmap_min_unitigs.end() ? 0 : pos - it_min_h.p);
        }

        pair<size_t, size_t> splitAllUnitigs();
        size_t joinAllUnitigs(vector<Kmer>* v_joins = nullptr, const size_t nb_threads = 1);

        bool checkJoin(const Kmer& a, const UnitigMap<T>& cm_a, Kmer& b);
        void check_fp_tips(KmerHashTable<bool>& ignored_km_tips);
        size_t removeUnitigs(bool rmIsolated, bool clipTips, vector<Kmer>& v);

        void writeGFA(string graphfilename, const size_t nb_threads = 1);
        void mapRead(const UnitigMap<T>& cc);

        size_t cstrMatch(const char* a, const char* b) const;
        inline size_t stringMatch(const string& a, const string& b, size_t pos);
};

#include "CompactedDBG.tpp"

#endif
