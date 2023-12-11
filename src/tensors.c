#include "tensors.h"

#include "jsmn.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

static bool json_equal(const char* json, const jsmntok_t* tok, const char* str) {
	assert(tok->type == JSMN_STRING);
	size_t len = tok->end - tok->start;
	return len == strlen(str) && memcmp(json + tok->start, str, len) == 0;
}

static long long json_llong(const char* json, const jsmntok_t* tok) {
	if (tok->type != JSMN_PRIMITIVE) {
		return 0;
	}

	char tmp[128];
	int size = (size_t)(tok->end - tok->start) < sizeof(tmp) ? (int)(tok->end - tok->start) : (int)(sizeof(tmp) - 1);
	strncpy(tmp, json + tok->start, size);
	tmp[size] = 0;
	return atoll(tmp);
}

static int parse_tensor(struct Tensor* tensor, void* bytes, size_t bytes_size, const char* json, const jsmntok_t* tokens, int toki) {
	assert(tokens[toki].type == JSMN_STRING);
	strncpy(tensor->name, json + tokens[toki].start, tokens[toki].end - tokens[toki].start);
	toki++;

	if (tokens[toki].type != JSMN_OBJECT) {
		return -1;
	}

	int n_keys = tokens[toki].size;
	toki++;

	for (int i = 0; i < n_keys; ++i) {
		const jsmntok_t* key = &tokens[toki];

		if (json_equal(json, key, "dtype") && tokens[toki + 1].type == JSMN_STRING) {
			if (json_equal(json, &tokens[toki + 1], "F32")) {
				tensor->dtype = dt_f32;
			} else {
				return -1;
			}
			toki += 2;
		} else if (json_equal(json, key, "shape") && tokens[toki + 1].type == JSMN_ARRAY && tokens[toki + 1].size <= 4) {
			int shape_len = tokens[toki + 1].size;
			for (int j = 0; j < shape_len; ++j) {
				tensor->shape[j] = json_llong(json, &tokens[toki + 2 + j]);
			}
			toki += 2 + shape_len;
		} else if (json_equal(json, key, "data_offsets") && tokens[toki + 1].type == JSMN_ARRAY && tokens[toki + 1].size == 2) {
			long long start = json_llong(json, &tokens[toki + 2]);
			long long end = json_llong(json, &tokens[toki + 3]);
			toki += 4;

			if (start < 0 || end <= start || (size_t)end > bytes_size) {
				return -1;
			}

			tensor->data = (char*)bytes + start;
		} else {
			return -1;
		}
	}

	return toki;
}

int tensors_open(struct Tensors* tensors, const char* filename) {
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		return -1;
	}

	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size < (int)sizeof(uint64_t)) {
		close(fd);
		return -1;
	}

	size_t size = st.st_size;
	void* data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return -1;
	}

	close(fd); // fd can be closed after mmap returns without invalidating the mapping

	uint64_t header_size = *(uint64_t*)data;
	if (header_size == 0 || header_size > size - sizeof(uint64_t)) {
		munmap(data, size);
		return -2;
	}

	char* header = (char*)data + sizeof(uint64_t);
	void* bytes = (char*)data + sizeof(uint64_t) + header_size;
	size_t bytes_size = size - sizeof(uint64_t) - header_size;

	jsmn_parser parser;
	jsmntok_t tokens[16384];
	int tokres = jsmn_parse(&parser, header, header_size, tokens, sizeof(tokens) / sizeof(tokens[0]));

	if (tokres <= 0 || tokens[0].type != JSMN_OBJECT) {
		munmap(data, size);
		return -3;
	}

	int toki = 1;
	while (toki < tokres) {
		if (json_equal(header, &tokens[toki], "__metadata__") && tokens[toki + 1].type == JSMN_OBJECT) {
			// TODO: validate
			toki += 2 + tokens[toki + 1].size * 2; // skip metadata, assume string => string mapping
		} else {
			struct Tensor tensor = {};
			toki = parse_tensor(&tensor, bytes, bytes_size, header, tokens, toki);
			if (toki < 0) {
				munmap(data, size);
				return -3;
			}

			if (tensors->n_tensors >= sizeof(tensors->tensors) / sizeof(tensors->tensors[0])) {
				munmap(data, size);
				return -4;
			}
			tensors->tensors[tensors->n_tensors++] = tensor;
		}
	}

	return 0;
}

void tensors_close(struct Tensors* tensors) {
	munmap(tensors->data, tensors->size);
}