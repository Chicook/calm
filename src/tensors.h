#pragma once

#include <stddef.h>

enum DType {
	dt_f32
};

struct Tensor {
	char name[64];
	enum DType dtype;
	int shape[4];
	void* data;
};

struct Tensors {
	void* data;
	size_t size;

	struct Tensor tensors[1024];
	size_t n_tensors;
};

int tensors_open(struct Tensors* tensors, const char* filename);
void tensors_close(struct Tensors* tensors);