/*  This file is part of Jellyfish.

    Jellyfish is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Jellyfish is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Jellyfish.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __JELLYFISH_PARSE_QUAKE_HPP__
#define __JELLYFISH_PARSE_QUAKE_HPP__

#include <iostream>
#include <vector>
#include <jellyfish/double_fifo_input.hpp>
#include <jellyfish/misc.hpp>
#include <jellyfish/seq_qual_parser.hpp>
#include <jellyfish/circular_buffer.hpp>

namespace jellyfish {
  class parse_quake : public double_fifo_input<seq_qual_parser::sequence_t> {
    typedef std::vector<const char *> fary_t;

    uint_t                  mer_len;
    size_t                  buffer_size;
    fary_t                  files;
    fary_t::const_iterator  current_file;
    bool                    have_seam;
    const char              quality_start;
    char                   *buffer_data;
    struct seq             *buffers;
    char                   *seam;
    bool                    canonical;
    seq_qual_parser        *fparser;

  public:
    /* Action to take for a given letter in fasta file:
     * A, C, G, T: map to 0, 1, 2, 3. Append to kmer
     * Other nucleic acid code: map to -1. reset kmer
     * '\n': map to -2. ignore
     * Other ASCII: map to -3. Skip to next line
     */
    static const uint_t codes[256];
    static const uint_t CODE_RESET = -1;
    static const float proba_codes[41];
    static const float one_minus_proba_codes[41];

    parse_quake(int nb_files, char *argv[], uint_t _mer_len, 
                unsigned int nb_buffers, size_t _buffer_size,
                const char _qs); 

    ~parse_quake() {
      delete [] buffer_data;
    }

    void set_canonical(bool v = true) { canonical = v; }
    virtual void fill();
    
    class thread {
      parse_quake            *parser;
      bucket_t               *sequence;
      const uint_t            mer_len, lshift;
      uint64_t                kmer, rkmer;
      const uint64_t          masq;
      uint_t                  cmlen;
      const bool              canonical;
      circular_buffer<float>  quals;
      const char              quality_start;

    public:
      thread(parse_quake *_parser, const char _qs) :
        parser(_parser), sequence(0),
        mer_len(_parser->mer_len), lshift(2 * (mer_len - 1)),
        kmer(0), rkmer(0), masq((1UL << (2 * mer_len)) - 1),
        cmlen(0), canonical(parser->canonical), quals(mer_len),
        quality_start(_qs) { }

      template<typename T>
      void parse(T &counter) {
        cmlen = kmer = rkmer = 0;
        while((sequence = parser->next())) {
          const char         *start = sequence->start;
          const char * const  end   = sequence->end;
          while(start < end) {
            const uint_t c = codes[(uint_t)*start++];
            const char   q = *start++;
            switch(c) {
            case CODE_RESET:
              cmlen = kmer = rkmer = 0;
              break;

            default:
              kmer = ((kmer << 2) & masq) | c;
              rkmer = (rkmer >> 2) | ((0x3 - c) << lshift);
              const float one_minus_p = one_minus_proba_codes[(uint_t)(q - quality_start)];
              quals.append(one_minus_p);
              if(++cmlen >= mer_len) {
                cmlen  = mer_len;
                if(canonical)
                  counter->add(kmer < rkmer ? kmer : rkmer, quals.prod());
                else
                  counter->add(kmer, quals.prod());
              }
            }
          }

          // Buffer exhausted. Get a new one
          cmlen = kmer = rkmer = 0;
          parser->release(sequence);
        }
      }
    };
    friend class thread;
    thread new_thread() { return thread(this, quality_start); }
  };
}

#endif
