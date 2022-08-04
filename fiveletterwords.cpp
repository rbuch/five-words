// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.

// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.

#include <cstddef>
#include <cstdint>
#include <chrono>

#include <algorithm>
#include <numeric>

#include <fstream>
#include <iostream>

#include <bitset>
#include <string>
#include <vector>

#ifdef __has_include
#if __has_include(<bit>)
#include <bit>
#endif
#endif

constexpr int WORD_LENGTH = 5;

int main(int argc, char *argv[]) {
  const auto start_time = std::chrono::steady_clock::now();
  if (argc < 2) {
    std::cerr << "Please provide wordlist filename!" << std::endl;
    return 1;
  }

  // First, open the word list given on the command line and read in some words!

  std::ifstream word_file(argv[1]);
  std::vector<std::string> word_list;

  if (word_file.is_open()) {
    std::string line;
    while (std::getline(word_file, line)) {
      // Trim any leading or trailing whitespace
      line.erase(line.begin(),
                 std::find_if(line.begin(), line.end(), [](unsigned char c) {
                   return !std::isspace(c);
                 }));
      line.erase(std::find_if(line.rbegin(), line.rend(),
                              [](unsigned char c) { return !std::isspace(c); })
                     .base(),
                 line.end());

      word_list.push_back(line);
    }
    word_file.close();
  } else {
    std::cerr << "Could not open file: " << argv[1] << std::endl;
    return 2;
  }

  std::cout << "Read " << word_list.size() << " words from " << argv[1]
            << std::endl;

  // Next, filter the word list down to the words we actually care about (i.e.
  // words of length five with no duplicate letters)

  // Create several mutually exclusive lists of words, the first for words with
  // an 'e', the next for words with a 't' but no 'e', the next for words with
  // an 'a' but no 't' or 'e', and so on, with an extra list at the end that's a
  // catch all for everything else. This allows us to more quickly prune the
  // search space later on since we can disregard the entire block if the
  // corresponding letter is present in the combined bitmap.

  // The number of individual letter lists to make is a potential trade off,
  // here are a few, but, empirically, the longest one 'etaoinshrldu' seems to
  // be best.
  // std::vector<char> letters = {'e'};
  // std::vector<char> letters = {'e', 't', 'a', 'o', 'i', 'n'};
  std::vector<char> letters = {'e', 't', 'a', 'o', 'i', 'n',
                               's', 'h', 'r', 'l', 'd', 'u'};
  std::vector<uint32_t> letter_bitmaps;
  std::transform(letters.cbegin(), letters.cend(),
                 std::back_inserter(letter_bitmaps),
                 [](char letter) { return 1 << (letter - 'a'); });
  letter_bitmaps.push_back(0xffff); // At the end, match against everything left
  std::vector<std::vector<uint32_t>> word_bitmaps_letters(
      letter_bitmaps.size());
  std::vector<std::vector<std::string>> unique_words_letters(
      letter_bitmaps.size());

  for (const auto &word : word_list) {
    if (word.length() != WORD_LENGTH)
      continue;

    // Use a bitmap to represent a set for performance. Since there are only 26
    // possible letters (assumes that all letters are lower case ASCII), the 32
    // bits of a uint32_t are sufficient. Set the bit corresponding to the index
    // of each character (e.g. 'a' = 0, 'b' = 1, ...).
    const uint32_t bitmap = std::accumulate(
        word.begin(), word.end(), 0,
        [](uint32_t bitmap, const char c) { return bitmap |= 1 << (c - 'a'); });

    // Get the number of bits set in the bitmap
#ifdef __cpp_lib_bitops
    // C++20 only feature
    const int bitcount = std::popcount(bitmap);
#else
    const std::bitset<8 * sizeof(uint32_t)> b(bitmap);
    const int bitcount = b.count();
#endif

    // If there are no duplicate characters, check which collection to add it to
    if (bitcount == WORD_LENGTH) {
      for (size_t i = 0; i < letter_bitmaps.size(); i++) {
        // If the current bitmap contains the given letter
        if ((bitmap & letter_bitmaps[i]) != 0) {
          // If we haven't seen this bitmap before
          if (word_bitmaps_letters[i].end() ==
              std::find(word_bitmaps_letters[i].begin(),
                        word_bitmaps_letters[i].end(), bitmap)) {
            word_bitmaps_letters[i].push_back(bitmap);
            unique_words_letters[i].push_back(word);
          }
          break;
        }
      }
    }
  }

  const size_t number_of_words = std::accumulate(
      word_bitmaps_letters.cbegin(), word_bitmaps_letters.cend(), 0,
      [](size_t sum, const std::vector<uint32_t> &vec) {
        return sum + vec.size();
      });

  std::vector<uint32_t> word_bitmaps;
  std::vector<std::string> unique_words;

  word_bitmaps.reserve(number_of_words);
  unique_words.reserve(number_of_words);

  std::vector<size_t> word_bitmaps_boundaries;

  // Combine each individual letter list into one, saving the boundaries for
  // later
  for (size_t i = 0; i < word_bitmaps_letters.size(); i++) {
    word_bitmaps_boundaries.push_back(word_bitmaps.size());
    word_bitmaps.insert(word_bitmaps.end(), word_bitmaps_letters[i].begin(),
                        word_bitmaps_letters[i].end());
    unique_words.insert(unique_words.end(), unique_words_letters[i].begin(),
                        unique_words_letters[i].end());
  }
  word_bitmaps_boundaries.push_back(word_bitmaps.size());

  // Reset this to 0 since we're looking for the opposite now, we want all words
  // to match with the last section
  letter_bitmaps[letter_bitmaps.size() - 1] = 0;

  std::cout << "Found " << word_bitmaps.size() << " unique words" << std::endl;

  // Finally, it's time to actually look for some words!

  std::vector<std::vector<std::string>> matches;

  std::vector<uint32_t> candidate_bitmaps;
  std::vector<size_t> candidate_indices;

#pragma omp parallel for schedule(dynamic) private(candidate_indices,          \
                                                   candidate_bitmaps)
  for (size_t i = 0; i < number_of_words; i++) {
    const auto used_i = word_bitmaps[i];

    for (size_t j = i + 1; j < number_of_words; j++) {
      if ((used_i & word_bitmaps[j]) != 0)
        continue;
      const auto used_ij = used_i | word_bitmaps[j];

      // Prune the remaining words down to a set of candidates that do not share
      // a letter with either of the two words we've seen so far
      candidate_bitmaps.clear();
      candidate_indices.clear();

      for (size_t index = 0; index < word_bitmaps_boundaries.size() - 1;
           index++) {
        // If this is 0, that means the given letter is not in used_ij, so
        // search through the corresponding section looking for candidates
        if ((letter_bitmaps[index] & used_ij) == 0) {
          for (size_t k = std::max(j + 1, word_bitmaps_boundaries[index]);
               k < word_bitmaps_boundaries[index + 1]; k++) {
            if ((used_ij & word_bitmaps[k]) == 0) {
              candidate_bitmaps.push_back(word_bitmaps[k]);
              candidate_indices.push_back(k);
            }
          }
        }
      }

      const auto num_candidates = candidate_bitmaps.size();
      if (num_candidates < WORD_LENGTH - 2)
        continue;

      // From here, only search through the pruned set of candidates
      for (size_t a = 0; a < num_candidates; a++) {
        const auto a_bitmap = candidate_bitmaps[a];
        const auto used_ijk = used_ij | a_bitmap;
        for (size_t b = a + 1; b < num_candidates; b++) {
          const auto b_bitmap = candidate_bitmaps[b];
          if ((used_ijk & b_bitmap) != 0)
            continue;
          const auto used_ijkl = used_ijk | b_bitmap;
          for (size_t c = b + 1; c < num_candidates; c++) {
            const auto c_bitmap = candidate_bitmaps[c];
            if ((used_ijkl & c_bitmap) != 0)
              continue;
#pragma omp critical
            {
              matches.push_back({unique_words[i], unique_words[j],
                                 unique_words[candidate_indices[a]],
                                 unique_words[candidate_indices[b]],
                                 unique_words[candidate_indices[c]]});
            }
          }
        }
      }
    }
  }
  std::cout << "Damn, we had " << matches.size() << " successful finds!"
            << std::endl
            << "Here they all are:" << std::endl;

  for (const auto &match : matches) {
    for (const auto &word : match) {
      std::cout << word << " ";
    }
    std::cout << std::endl;
  }

  const auto end_time = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  std::cout << "DONE in " << elapsed.count() / 1000.0 << " seconds" << std::endl;

}
