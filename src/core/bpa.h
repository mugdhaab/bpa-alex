template <typename T, typename P>
class BPA : public AlexNode<T, P> {
private:
    int log_size;
    int num_blocks;
    int block_size;
    pair<T, P>* log;
    int log_count;
    float* header;
    pair<T, P>** blocks;

    int _find_block_idx(T key) {
        for (int i = 0; i < num_blocks; ++i) {
            if (key < header[i]) {
                return (i != 0) ? i - 1 : 0;
            }
        }
        return num_blocks - 1;
    }

    void _flush_log() {
		vector<int> count_per_block(num_blocks);
		for (int i = 0; i < num_blocks; ++i) {
			count_per_block[i] = 0;
			for (int j = 0; blocks[i][j].first != INF && j < block_size; ++j) {
				++count_per_block[i];
			}
		}

		vector<int> new_destined_per_block(num_blocks, 0);

		for (int i = 0; i < log_count; ++i) {
			int idx = _find_block_idx(log[i].first);
			new_destined_per_block[idx]++;
		}

		int no_space_idx = -1;

		for (int i = 0; i < num_blocks; ++i) {
			if (count_per_block[i] + new_destined_per_block[i] >= block_size) {
				no_space_idx = i;
				break;
			}
		}

		if (no_space_idx == -1) {
			// Case 2a
			for (int i = 0; i < log_count; ++i) {
				int idx = _find_block_idx(log[i].first);
				bool found = false;
				for (int j = 0; j < block_size; ++j) {
					if (blocks[idx][j].first == log[i].first) {
						blocks[idx][j] = log[i];
						found = true;
						break;
					}
				}
				if (!found) {
					// Append to the block if not found
					blocks[idx][count_per_block[idx]++] = log[i];
				}
			}
			log_count = 0;
		} else {
			// Case 2b
			vector<pair<T, P>> merged;
			for (int i = 0; i < log_count; ++i) {
				merged.push_back(log[i]);
			}
			for (int i = 0; i < num_blocks; ++i) {
				for (int j = 0; j < block_size; ++j) {
					if (blocks[i][j].first != INF) {
						merged.push_back(blocks[i][j]);
					}
				}
			}
			sort(merged.begin(), merged.end());

			while (merged.size() < static_cast<size_t>(num_blocks * block_size)) {
				merged.emplace_back(INF, P());
			}

			for (int i = 0; i < num_blocks; ++i) {
				header[i] = merged[i * block_size].first;
				for (int j = 0; j < block_size; ++j) {
					blocks[i][j] = merged[i * block_size + j];
				}
			}

			log_count = 0;
		}
	}


public:
    BPA(int log_size, int num_blocks, int block_size) 
        : log_size(log_size), num_blocks(num_blocks), block_size(block_size), log_count(0) {
        header = new float[num_blocks];
        fill(header, header + num_blocks, INF);

        log = new pair<T, P>[log_size];

        blocks = new pair<T, P>*[num_blocks];
        for (int i = 0; i < num_blocks; ++i) {
            blocks[i] = new pair<T, P>[block_size];
        }
    }

    ~BPA() {
        delete[] header;
        delete[] log;
        for (int i = 0; i < num_blocks; ++i) {
            delete[] blocks[i];
        }
        delete[] blocks;
    }

  //   void insert(T key, P value) {
  //     for (int i = 0; i < log_count; ++i) {
  //       if (log[i].first == key) {
  //         log[i].second = value;
  //         return;
  //       }
  //     }

  //     if (log_count < log_size) {
  //       log[log_count++] = make_pair(key, value);
  //     }

  //     if (log_count == log_size) {
  //       _flush_log();
  //     }
	// }


  std::pair<int, int> insert(const T& key, const P& value) {
      // Check if the key already exists in the log
      for (int i = 0; i < log_count; ++i) {
          if (log[i].first == key) {
              // Key already exists, duplicates not allowed
              return std::make_pair(-1, i);
          }
      }

      // Check if the log is at its maximum capacity
      if (log_count == log_size) {
          return std::make_pair(3, -1);
      }

      // Assuming other failure conditions are not applicable in this context
      // Insert the key-value pair
      log[log_count] = std::make_pair(key, value);
      log_count++;

      // Check if the log needs flushing after the insert
      if (log_count == log_size) {
          _flush_log(); // Flush the log if it reaches its capacity
      }

      // Successful insert, return position of the inserted key
      return std::make_pair(0, log_count - 1);
  }



  //   P* find(T key) {
	// 	for (int i = 0; i < log_count; ++i) {
	// 		if (log[i].first == key) {
	// 			return &log[i].second;
	// 		}
	// 	}

	// 	int block_idx = _find_block_idx(key);
	// 	for (int i = 0; i < block_size; ++i) {
	// 		if (blocks[block_idx][i].first == key) {
	// 			return &blocks[block_idx][i].second;
	// 		}
	// 	}

	// 	return nullptr;
	// }

  int find_key(const T& key) {
      // Search in the log
      for (int i = 0; i < log_count; ++i) {
          if (log[i].first == key) {
              return i;  // Key found in the log
          }
      }

      // Search in the blocks
      int block_idx = _find_block_idx(key);
      for (int i = 0; i < block_size; ++i) {
          if (blocks[block_idx][i].first == key) {
              return block_idx * block_size + i;  // Key found in a block
          }
      }

      return -1;  // Key not found
  }



    void iterate_range(T start, int length, function<void(pair<T, P>)> func) {
		// First, sort the log
		sort(log, log + log_count);

		int block_idx = _find_block_idx(start);
		
		// Sort the block
		sort(blocks[block_idx], blocks[block_idx] + block_size);

		int log_ptr = 0, block_ptr = 0;
		int processed = 0;

		while (processed < length) {
			if (log_ptr < log_count && 
				(block_ptr >= block_size || 
				log[log_ptr].first < blocks[block_idx][block_ptr].first)) {
				func(log[log_ptr]);
				log_ptr++;
			} else {
				if (block_ptr < block_size) {
					func(blocks[block_idx][block_ptr]);
					block_ptr++;
				}
			}

			processed++;

			if (block_ptr == block_size && processed < length) {
				block_idx++;
				if (block_idx == num_blocks) {
					break;
				}
				sort(blocks[block_idx], blocks[block_idx] + block_size);
				block_ptr = 0;
			}
		}
	}

};
