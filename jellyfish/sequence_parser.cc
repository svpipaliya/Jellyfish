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

#include <jellyfish/sequence_parser.hpp>

jellyfish::sequence_parser *
jellyfish::sequence_parser::new_parser(const char *path) {
  int fd;
  char peek;
  fd = file_peek(path, &peek);
  
  switch(peek) {
  case '>': return new jellyfish::fasta_sequence_parser(fd, path, &peek, 1);
  case '@': return new jellyfish::fastq_sequence_parser(fd, path, &peek, 1);
      
  default:
    eraise(FileParserError) << "Invalid input file '" 
                            << path << "'" << err::no;
  }

  return 0;
}

bool jellyfish::fasta_sequence_parser::parse(char *start, char **end) {
  while(start < *end && base() != EOF) {
    switch(sbumpc()) {
    case EOF:
      break;

    case '>':
      if(pbase() == '\n') {
        while(base() != EOF && base() != '\n') { sbumpc(); }
        *start++ = 'N';
      } else
        *start++ = base();
      break;

    case '\n':
      break;

    default:
      *start++ = base();
    }
  }

  *end = start;
  return base() != EOF;
}

bool jellyfish::fastq_sequence_parser::parse(char *start, char **end) {
  while(start < *end && base() != EOF) {
    switch(sbumpc()) {
    case EOF:
      break;

    case '@':
      if(pbase() == '\n') {
        while(base() != EOF && base() != '\n') { sbumpc(); }
        *start++ = 'N';
        seq_len = 0;
      } else
        *start++ = base();
      break;

    case '+':
      if(pbase() == '\n') { // Skip qual header & values
        while(base() != EOF && base() != '\n') { sbumpc(); }
        while(base() != EOF && seq_len > 0) {
          if(base() != '\n')
            --seq_len;
          sbumpc();
        }
      } else
        *start++ = base();
      break;

    case '\n':
      break;

    default:
      *start++ = base();
      ++seq_len;
    }
  }
    
  *end = start;
  return base() != EOF;
}
