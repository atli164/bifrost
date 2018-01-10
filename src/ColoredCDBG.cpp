#include "ColoredCDBG.hpp"

ColoredCDBG::ColoredCDBG(int kmer_length, int minimizer_length) : CompactedDBG(kmer_length, minimizer_length), color_sets(nullptr), nb_color_sets(0) {

    std::random_device rd; //Seed
    std::default_random_engine generator(rd()); //Random number generator
    std::uniform_int_distribution<long long unsigned> distribution(0,0xFFFFFFFFFFFFFFFF); //Distribution on which to apply the generator

    //Initialize the hash function seeds for
    for (int i = 0; i < 256; ++i) seeds[i] = distribution(generator);
}

ColoredCDBG::~ColoredCDBG() {

    if (color_sets != nullptr){

        delete[] color_sets;
        color_sets = nullptr;
    }
}

bool ColoredCDBG::build(const CDBG_Build_opt& opt){

    CompactedDBG::build(opt);

    initColorSets();
    mapColors(opt);

    /*size_t cpt = 0;

    for (auto& unitig : *this){

        if (getColorSet(unitig)->size() == (2 * (unitig.size - getK() + 1))){

            cout << unitig.toString() << endl;

            ++cpt;
        }
    }

    cout << "Number of unitigs where all k-mers have 2 colors is " << cpt << " on " << size() << " unitigs." << endl;*/

    return true;
}

void ColoredCDBG::initColorSets(const size_t max_nb_hash){

    int i;

    uint64_t h_v;

    size_t nb_unitig_not_hashed = 0;

    nb_color_sets = size();
    color_sets = new ColorSet[nb_color_sets];

    for (auto& unitig : *this){

        const Kmer head = unitig.getHead();

        for (i = 0; i < max_nb_hash; ++i){

            h_v = head.hash(seeds[i]) % nb_color_sets;

            if (color_sets[h_v].isUnoccupied()) break;
        }

        const HashID hid(static_cast<uint8_t>(i == max_nb_hash ? 0 : i + 1));
        unitig.setData(&hid);

        if (i == max_nb_hash) km_overflow.insert(head, ColorSet());
        else color_sets[h_v].setOccupied();
    }

    cout << "Number of unitigs not hashed is " << km_overflow.size() << " on " << nb_color_sets << " unitigs." << endl;
}

void ColoredCDBG::mapColors(const CDBG_Build_opt& opt){

    const int k_ = getK();

    size_t file_id = 0;

    string s;

    vector<string> readv;

    FastqFile FQ(opt.fastx_filename_in);

    // Main worker thread
    auto worker_function = [&](vector<string>::iterator a, vector<string>::iterator b) {

        // for each input
        for (auto x = a; x != b; ++x) {

            for (KmerIterator it_km(x->c_str()), it_km_end; it_km != it_km_end; ++it_km) {

                UnitigMap<HashID> um = find(it_km->first);

                if (!um.isEmpty) {

                    if (um.strand || (um.dist != 0)){

                        um.len += um.lcp(x->c_str(), it_km->second + k_, um.strand ? um.dist + k_ : um.dist - 1, um.strand);
                        it_km += um.len - 1;
                    }

                    HashID* hid = um.getData();

                    hid->lock();

                    setColor(um, file_id);

                    hid->unlock();
                }
            }
        }
    };

    bool done = false;

    while (!done) {

        size_t reads_now = 0;

        while (reads_now < opt.read_chunksize) {

            if (FQ.read_next(s, file_id) >= 0){

                readv.push_back(s);
                ++reads_now;
            }
            else {

                done = true;
                break;
            }
        }

        // run parallel code
        vector<thread> workers;

        auto rit = readv.begin();
        size_t batch_size = readv.size() / opt.nb_threads;
        size_t leftover   = readv.size() % opt.nb_threads;

        for (size_t i = 0; i < opt.nb_threads; ++i) {

            size_t jump = batch_size + (i < leftover ? 1 : 0);
            auto rit_end(rit);

            advance(rit_end, jump);
            workers.push_back(thread(worker_function, rit, rit_end));

            rit = rit_end;
        }

        assert(rit == readv.end());

        for (auto& t : workers) t.join();

        readv.clear();
    }

    FQ.close();
}

bool ColoredCDBG::setColor(const UnitigMap<HashID>& um, const size_t color_id) {

    if (!um.isEmpty && (color_sets != nullptr)){

        ColorSet* color_set = getColorSet(um);

        if (color_set != nullptr){

            color_set->add(um, color_id);

            return true;
        }
    }

    return false;
}

bool ColoredCDBG::joinColors(const UnitigMap<HashID>& um_dest, const UnitigMap<HashID>& um_src) {

    if (!um_dest.isEmpty && !um_src.isEmpty && (color_sets != nullptr)){

        ColorSet* color_set_dest = getColorSet(um_dest);
        ColorSet* color_set_src = getColorSet(um_src);

        if ((color_set_dest != nullptr) && (color_set_src != nullptr)){

            ColorSet new_cs, csd_rev, css_rev;

            size_t prev_color_id = 0xffffffffffffffff;
            size_t prev_km_dist = 0xffffffffffffffff;

            const size_t um_dest_km_sz = um_dest.size - getK() + 1;
            const size_t um_src_km_sz = um_src.size - getK() + 1;

            UnitigMap<HashID> new_um_dest(um_dest.pos_unitig, 0, 0, um_dest.size + um_src.size,
                                          um_dest.isShort, um_dest.isAbundant, um_dest.strand, *(um_dest.cdbg));

            if (!um_dest.strand){

                csd_rev = color_set_dest->reverse(um_dest.size);
                color_set_dest = &csd_rev;
            }

            ColorSet::const_iterator it = color_set_dest->begin(), it_end = color_set_dest->end();

            if (it != it_end){

                prev_color_id = *it / um_dest_km_sz;
                prev_km_dist = *it - (prev_color_id * um_dest_km_sz);

                new_um_dest.dist = prev_km_dist;
                new_um_dest.len = 1;

                ++it;
            }

            // Insert colors layer by layer
            for (; it != it_end; ++it){

                const size_t color_id = *it / um_dest_km_sz;
                const size_t km_dist = *it - (color_id * um_dest_km_sz);

                if ((color_id != prev_color_id) || (km_dist != prev_km_dist + 1)){

                    new_cs.add(new_um_dest, color_id);

                    new_um_dest.dist = km_dist;
                    new_um_dest.len = 1;
                }
                else ++new_um_dest.len;

                prev_color_id = color_id;
                prev_km_dist = km_dist;
            }

            if (new_um_dest.dist + new_um_dest.len != 0) new_cs.add(new_um_dest, prev_color_id);

            UnitigMap<HashID> new_um_src(um_src.pos_unitig, 0, 0, um_dest.size + um_src.size,
                                         um_src.isShort, um_src.isAbundant, um_src.strand, *(um_src.cdbg));

            if (!um_src.strand){

                css_rev = color_set_src->reverse(um_src.size);
                color_set_src = &css_rev;
            }

            it = color_set_src->begin(), it_end = color_set_src->end();

            if (it != it_end){

                prev_color_id = *it / um_src_km_sz;
                prev_km_dist = *it - (prev_color_id * um_src_km_sz);

                new_um_src.dist = prev_km_dist + um_dest.size;
                new_um_src.len = 1;

                ++it;
            }

            // Insert colors layer by layer
            for (; it != it_end; ++it){

                const size_t color_id = *it / um_src_km_sz;
                const size_t km_dist = *it - (color_id * um_src_km_sz);

                if ((color_id != prev_color_id) || (km_dist != prev_km_dist + 1)){

                    new_cs.add(new_um_src, color_id);

                    new_um_src.dist = km_dist + um_dest.size;
                    new_um_src.len = 1;
                }
                else ++new_um_src.len;

                prev_color_id = color_id;
                prev_km_dist = km_dist;
            }

            if (new_um_src.dist + new_um_src.len != 0) new_cs.add(new_um_src, prev_color_id);

            *color_set_dest = new_cs;

            return true;
        }
    }

    return false;
}

ColorSet ColoredCDBG::extractColors(const UnitigMap<HashID>& um) const {

    ColorSet new_cs;

    if (!um.isEmpty && (color_sets != nullptr)){

        const ColorSet* cs = getColorSet(um);

        if (cs != nullptr){

            const size_t end = um.dist + um.len;
            const size_t um_km_sz = um.size - getK() + 1;

            UnitigMap<HashID> fake_um(0, 0, 1, um.len, false, false, um.strand, *(um.cdbg));

            ColorSet::const_iterator it = cs->begin(), it_end = cs->end();

            for (ColorSet::const_iterator it = cs->begin(), it_end = cs->end(); it != it_end; ++it){

                const size_t color_id = *it / um_km_sz;
                const size_t km_dist = *it - (color_id * um_km_sz);

                if ((km_dist >= um.dist) && (km_dist < end)){

                    fake_um.dist = km_dist - um.dist;

                    new_cs.add(fake_um, color_id);
                }
            }
        }
    }

    return new_cs;
}

ColorSet* ColoredCDBG::getColorSet(const UnitigMap<HashID>& um) {

    ColorSet* color_set = nullptr;

    if (!um.isEmpty && (color_sets != nullptr)){

        const Kmer head = um.getHead();

        const uint8_t hash_id = um.getData()->get();

        if (hash_id == 0){

            KmerHashTable<ColorSet>::iterator it = km_overflow.find(head);

            if (it != km_overflow.end()) color_set = &(*it);
        }
        else color_set = &color_sets[head.hash(seeds[hash_id - 1]) % nb_color_sets];
    }

    return color_set;
}

const ColorSet* ColoredCDBG::getColorSet(const UnitigMap<HashID>& um) const {

    if (!um.isEmpty && (color_sets != nullptr)){

        const Kmer head = um.getHead();

        const uint8_t hash_id = um.getData()->get();

        if (hash_id == 0){

            KmerHashTable<ColorSet>::const_iterator it = km_overflow.find(head);

            if (it != km_overflow.end()) return &(*it);
        }
        else return &color_sets[head.hash(seeds[hash_id - 1]) % nb_color_sets];
    }

    return nullptr;
}
