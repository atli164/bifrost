#ifndef BFG_COLOREDCDBG_HPP
#define BFG_COLOREDCDBG_HPP

#include <iostream>
#include <random>
#include <unordered_set>

#include "CompactedDBG.hpp"
#include "DataManager.hpp"

/** @file src/ColoredCDBG.hpp
* Interface for the Colored and Compacted de Bruijn graph API.
* Code snippets using this interface are provided in test/snippets.hpp.
*/

/** @struct CCDBG_Build_opt
* @brief Members of this structure are parameters for ColoredCDBG::build, except for:
* - CCDBG_Build_opt::k and CCDBG_Build_opt::g as they must be set in the constructor of the graph.
* - CCDBG_Build_opt::clipTips, CCDBG_Build_opt::deleteIsolated and CCDBG_Build_opt::useMercyKmers are used
* by ColoredCDBG::simplify
* - CCDBG_Build_opt::prefixFilenameOut, CCDBG_Build_opt::outputGFA and CCDBG_Build_opt::outputColors are
* used by ColoredCDBG::write
* Most parameters have default values.
* An example of using such a structure is shown in src/Bifrost.cpp.
* @var CCDBG_Build_opt::reference_mode
* Input are assembled genomes, unitigs or graph files (true), no filtering step performed, all k-mers are used.
* You must not directly build a ColoredCDBG from reads (check README file).
* Default is true.
* @var CCDBG_Build_opt::verbose
* Print information messages during execution if true. Default is false.
* @var CCDBG_Build_opt::clipTips
* Clip short (length < 2k) tips of the graph (not used by ColoredCDBG::build). Default is false.
* @var CCDBG_Build_opt::deleteIsolated
* Remove short (length < 2k) isolated unitigs of the graph (not used by ColoredCDBG::build).
* Default is false.
* @var CCDBG_Build_opt::useMercyKmers
* Keep in the graph low coverage k-mers connecting tips of the graph. Default is false.
* @var CCDBG_Build_opt::k
* Length of k-mers (not used by ColoredCDBG::build). Default is 31.
* @var CCDBG_Build_opt::g
* Length of g-mers, the minimizers, such that g < k (not used by ColoredCDBG::build).
* Default is 23.
* @var CCDBG_Build_opt::nb_threads
* Number of threads to use for building the graph. Default is 1.
* @var CCDBG_Build_opt::read_chunksize
* Number of reads shared and processed by CCDBG_Build_opt::nb_threads threads at the same time.
* Default is 10000.
* @var CCDBG_Build_opt::unitig_size
* Maximum length of a unitig. Default is 100000.
* @var CCDBG_Build_opt::nb_unique_kmers
* Estimated number (upper bound) of different k-mers in the input FASTA/FASTQ/GFA files of
* CCDBG_Build_opt::filename_in. Default is KmerStream estimation.
* @var CCDBG_Build_opt::nb_non_unique_kmers
* Estimated number (upper bound) of different k-mers occurring twice or more in the FASTA/FASTQ/GFA
* files of CCDBG_Build_opt::filename_in. Default is KmerStream estimation.
* @var CCDBG_Build_opt::nb_bits_unique_kmers_bf
* Number of Bloom filter bits per k-mer occurring at least once in the FASTA/FASTQ/GFA files of
* CCDBG_Build_opt::filename_in. Default is 14.
* @var CCDBG_Build_opt::nb_bits_non_unique_kmers_bf
* Number of Bloom filter bits per k-mer occurring at least twice in the FASTA/FASTQ/GFA files of
* CCDBG_Build_opt::filename_in. Default is 14.
* @var CCDBG_Build_opt::prefixFilenameOut
* Prefix for the name of the file to which the graph must be written. Mandatory parameter.
* @var CCDBG_Build_opt::inFilenameBBF
* String containing the name of a Bloom filter file that is generated by ColoredCDBG::filter.
* If empty, the filtering step is executed. Otherwise, the Bloom filter is loaded from this file
* and the filtering step is skipped. Default is no input file.
* @var CCDBG_Build_opt::outFilenameBBF
* String containing the name of a Bloom filter file that will be generated by ColoredCDBG::filter.
* If empty, the file is not created. Otherwise, the Bloom filter is written to this file.
* Default is no output file.
* @var CCDBG_Build_opt::filename_seq_in
* vector of strings, each string is the name of a FASTA/FASTQ/GFA file to use for the graph construction.
* Mandatory parameter.
* @var CCDBG_Build_opt::filename_colors_in
* vector of strings, each string is the name of a color set file to use for the graph construction.
* If not empty, colors are loaded in memory from the files. In such case, the color filenames order
* in this vector must match the order of the filenames in CCDBG_Build_opt::filename_seq_in.
* Default is no input color files.
* @var CCDBG_Build_opt::outputGFA
* Boolean indicating if the graph is written to a GFA file (true) or if the unitigs are written to a
* FASTA file (false). Default is true.
* @var CCDBG_Build_opt::outputColors
* Boolean indicating if color sets must be written to disk (true) or not (false). Default is true.
*/
struct CCDBG_Build_opt {

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

    vector<string> filename_seq_in;

    // The following members are not used by CompactedDBG<T>::build
    // but you can set them to use them as parameters for other functions
    // such as CompactedDBG<T>::simplify or CompactedDBG<T>::write.

    vector<string> filename_colors_in;

    size_t k, g;

    bool clipTips;
    bool deleteIsolated;
    bool useMercyKmers;

    bool outputGFA;
    bool outputColors;

    string prefixFilenameOut;

    CCDBG_Build_opt() : nb_threads(1), k(DEFAULT_K), g(DEFAULT_G), nb_unique_kmers(0), nb_non_unique_kmers(0),
                        nb_bits_unique_kmers_bf(14), nb_bits_non_unique_kmers_bf(14), read_chunksize(10000),
                        unitig_size(1000000), verbose(false), clipTips(false), deleteIsolated(false), useMercyKmers(false),
                        outputGFA(true), outputColors(true), reference_mode(true), inFilenameBBF(""), outFilenameBBF("") {}

    /** Creates a CDBG_Build_opt (for a CompactedDBG object) from a CCDBG_Build_opt.
    * @return a CDBG_Build_opt
    */
    CDBG_Build_opt getCDBG_Build_opt() const {

        CDBG_Build_opt cdbg_opt;

        cdbg_opt.reference_mode = reference_mode;
        cdbg_opt.filename_in = filename_seq_in;

        cdbg_opt.verbose = verbose;

        cdbg_opt.nb_threads = nb_threads;
        cdbg_opt.read_chunksize = read_chunksize;
        cdbg_opt.unitig_size = unitig_size;
        cdbg_opt.nb_unique_kmers = nb_unique_kmers;
        cdbg_opt.nb_non_unique_kmers = nb_non_unique_kmers;
        cdbg_opt.nb_bits_unique_kmers_bf = nb_bits_unique_kmers_bf;
        cdbg_opt.nb_bits_non_unique_kmers_bf = nb_bits_non_unique_kmers_bf;

        cdbg_opt.inFilenameBBF = inFilenameBBF;
        cdbg_opt.outFilenameBBF = outFilenameBBF;

        cdbg_opt.clipTips = clipTips;
        cdbg_opt.deleteIsolated = deleteIsolated;
        cdbg_opt.useMercyKmers = useMercyKmers;

        cdbg_opt.outputGFA = outputGFA;

        cdbg_opt.prefixFilenameOut = prefixFilenameOut;

        return cdbg_opt;
    }
};

template<typename U> using UnitigColorMap = UnitigMap<DataAccessor<U>, DataStorage<U>>;
template<typename U> using const_UnitigColorMap = const_UnitigMap<DataAccessor<U>, DataStorage<U>>;

/** @class CCDBG_Data_t
* @brief If data are to be associated with the unitigs of the colored and compacted de Bruijn graph, those data
* must be wrapped into a class that inherits from the abstract class CCDBG_Data_t. Otherwise it will not compile.
* To associate data of type myUnitigData to unitigs, class myUnitigData must be declared as follows:
* \code{.cpp}
* class myUnitigData : public CCDBG_Data_t<myUnitigData> { ... };
* ...
* ColoredCDBG<myUnitigData> ccdbg;
* \endcode
* CCDBG_Data_t has one template parameters: the type of unitig data.
* Because CCDBG_Data_t is an abstract class, methods CCDBG_Data_t::join, CCDBG_Data_t::split and CCDBG_Data_t::serialize
* must be implemented in your wrapper. IMPORTANT: If you don't overload those methods, default ones that have no effects
* will be applied!
* An example of using such a structure is shown in snippets/test.cpp.
*/
template<typename Unitig_data_t> //Curiously Recurring Template Pattern (CRTP)
class CCDBG_Data_t {

    typedef Unitig_data_t U;

    public:

        /** Join data of two colored unitigs (each represented with a UnitigColorMap given as parameter) which are going to be
        * concatenated. Specifically, if A is the unitig represented by parameter um_dest and B is the unitig represented by
        * parameter um_src then, after the call to this function, A will become the concatenation of itself with B (A = AB) and
        * B will be removed. Be careful that if um_dest.strand = false, then the reverse-complement of A is going to be used in
        * the concatenation. Reciprocally, if um_src.strand = false, then the reverse-complement of B is going to be used in the
        * concatenation. The data of each unitig can be accessed through the method UnitigColorMap::getData().
        * @param um_dest is a UnitigColorMap object representing a colored unitig (and its data) to which another unitig is
        * going to be appended.
        * @param um_src is a UnitigColorMap object representing a colored unitig (and its data) that will be appended at the end
        * of the unitig represented by parameter um_dest.
        */
        static void join(const UnitigColorMap<U>& um_dest, const UnitigColorMap<U>& um_src){}

        /** Extract data from a colored unitig A to be associated with a colored unitig B which is the unitig mapping given by the
        * UnitigMap object um_src. Hence, B = A[um_src.dist, um_src.dist + um_src.len + k - 1]. Be careful that if um_src.strand is
        * false, then B will be extracted from the reverse-complement of A, i.e, B = rev(A[um_src.dist, um_src.dist + um_src.len + k - 1]).
        * @param um_src is a UnitigColorMap object representing the mapping to a colored unitig A from which a new colored unitig B
        * will be created, i.e, B = A[um_src.dist, um_src.dist + um_src.len + k - 1] or B = rev(A[um_src.dist, um_src.dist + um_src.len + k - 1])
        * if um_src.strand == false.
        * @param new_data is a pointer to a newly constructed object that you can fill in with new data to associate with colored unitig B.
        * @param last_extraction is a boolean indicating if this is the last call to this function on the reference unitig used for the
        * mapping given by um_src. If last_extraction is true, the reference unitig of um_src will be removed from the graph right after
        * this function returns.
        */
        static void sub(const UnitigColorMap<U>& um_src, U* new_data, bool last_extraction){}

        /** Serialize the data to a string. This function is used when the graph is written to disk in GFA format.
        * If the returned string is not empty, the string is appended to an optional field of the Segment line matching the unitig
        * of this data. If the returned string is empty, no optional field and string are appended to the Segment line matching the
        * unitig of this data.
        */
        string serialize() const { return string(); }
};

/** @class ColoredCDBG
* @brief Represent a Colored and Compacted de Bruijn graph. The class inherits from CompactedDBG
* which means that all public functions available with CompactedDBG are also available with ColoredCDBG.
* ColoredCDBG<> ccdbg_1; // No unitig data
* ColoredCDBG<void> ccdbg_2; // Equivalent to previous notation
* ColoredCDBG<myUnitigData> ccdbg_3; // An object of type myUnitigData will be associated with each unitig (along the colors)
* \endcode
* If data are to be associated with the unitigs, these data must be wrapped into a class that inherits from the abstract class
* CCDBG_Data_t, such as in:
* \code{.cpp}
* class myUnitigData : public CCDBG_Data_t<myUnitigData> { ... };
* CompactedDBG<myUnitigData> cdbg;
* \endcode
*/
template<typename Unitig_data_t = void>
class ColoredCDBG : public CompactedDBG<DataAccessor<Unitig_data_t>, DataStorage<Unitig_data_t>> {

    static_assert(is_void<Unitig_data_t>::value || is_base_of<CCDBG_Data_t<Unitig_data_t>, Unitig_data_t>::value,
                  "Type Unitig_data_t of data associated with vertices of class ColoredCDBG<Unitig_data_t> must "
                  " be void (no data) or a class extending class CCDBG_Data_t");

    typedef Unitig_data_t U;

    template<typename U> friend class DataAccessor;

    public:

        /** Constructor (set up an empty colored cdBG).
        * @param kmer_length is the length k of k-mers used in the graph (each unitig is of length at least k).
        * @param minimizer_length is the length g of minimizers (g < k) used in the graph.
        */
        ColoredCDBG(int kmer_length = DEFAULT_K, int minimizer_length = DEFAULT_G);

        /** Copy constructor (copy a colored cdBG).
        * This function is expensive in terms of time and memory as the content of a colored and compacted
        * de Bruijn graph is copied. After the call to this function, the same graph exists twice in memory.
        * @param o is a constant reference to the colored and compacted de Bruijn graph to copy.
        */
        ColoredCDBG(const ColoredCDBG& o);

        /** Move constructor (move a colored cdBG).
        * The content of o is moved ("transfered") to a new colored and compacted de Bruijn graph.
        * The colored and compacted de Bruijn graph referenced by o will be empty after the call to this constructor.
        * @param o is a reference on a reference to the colored and compacted de Bruijn graph to move.
        */
        ColoredCDBG(ColoredCDBG&& o);

        /** Copy assignment operator (copy a colored cdBG).
        * This function is expensive in terms of time and memory as the content of a colored and compacted
        * de Bruijn graph is copied.  After the call to this function, the same graph exists twice in memory.
        * @param o is a constant reference to the colored and compacted de Bruijn graph to copy.
        * @return a reference to the colored and compacted de Bruijn which is the copy.
        */
        ColoredCDBG& operator=(const ColoredCDBG& o);

        /** Move assignment operator (move a colored cdBG).
        * The content of o is moved ("transfered") to a new colored and compacted de Bruijn graph.
        * The colored and compacted de Bruijn graph referenced by o will be empty after the call to this operator.
        * @param o is a reference on a reference to the colored and compacted de Bruijn graph to move.
        * @return a reference to the colored and compacted de Bruijn which has (and owns) the content of o.
        */
        ColoredCDBG& operator=(ColoredCDBG&& o);

        /** Clear the graph: empty the graph and reset its parameters.
        */
        void clear();

        /** Build the Colored and compacted de Bruijn graph (only the unitigs).
        * A call to ColoredCDBG::mapColors is required afterwards to map colors to unitigs.
        * @param opt is a structure from which the members are parameters of this function. See CCDBG_Build_opt.
        * @return boolean indicating if the graph has been built successfully.
        */
        bool build(const CCDBG_Build_opt& opt);

        /** Map the colors to the unitigs. This is done by reading the input files and querying the graph.
        * If a color filename is provided in opt.filename_colors_in, colors are loaded from that file instead.
        * @param opt is a structure from which the members are parameters of this function. See CCDBG_Build_opt.
        * @return boolean indicating if the colors have been mapped successfully.
        */
        bool mapColors(const CCDBG_Build_opt& opt);

        /** Write a colored and compacted de Bruijn graph to disk.
        * @param prefix_output_filename is a string which is the prefix of the filename for the two files that are
        * going to be written to disk. Assuming the prefix is "XXX", two files "XXX.gfa" and "XXX.bfg_colors" will
        * be written to disk.
        * @param nb_threads is the number of threads that can be used to write the graph to disk.
        * @param verbose is a boolean indicating if information message are printed during writing (true) or not (false).
        * @return a boolean indicating if the graph was successfully written.
        */
        bool write(const string prefix_output_filename, const size_t nb_threads = 1, const bool verbose = false);

        /** Get the name of a color. As colors match the input files, the color names match the input filenames.
        * @return a string which is either a color name or an error message if the color ID is invalid or if the
        * colors have not yet been mapped to the unitigs.
        */
        string getColorName (const size_t color_id) const;

        /** Get the number of colors in the graph.
        * @return the number of colors in the graph.
        */
        inline size_t getNbColors() const { return this->getData()->getNbColors(); }

    private:

        void checkColors(const CCDBG_Build_opt& opt);

        void initColorSets(const CCDBG_Build_opt& opt, const size_t max_nb_hash = 31);
        void buildColorSets(const size_t nb_threads);

        inline bool readColorSets(const CCDBG_Build_opt& opt){

            return this->getData()->read(opt.filename_colors_in[0], opt.verbose);
        }

        bool invalid;
};

#include "ColoredCDBG.tcc"

#endif
