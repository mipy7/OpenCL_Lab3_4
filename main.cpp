// Сортировка большого количества маленьких массивов(по 32 - 256 элементов) одинаковой длинны(размер данных 100МБ)
// memory speed ~110Gb/s
// Teoretical time max( 2ms (read + write), 2.6 ms) = 2.6 ms

#include <iostream>
#include <CL/cl.h>
#include <chrono>

#define checkError(func) \
  if (errcode != CL_SUCCESS)\
  {\
    printf("Error in " #func "\nError code = %d\n", errcode);\
    exit(1);\
  }

#define checkErrorEx(command) \
  command; \
  checkError(command);

using namespace std;

int main() {
	setlocale(LC_ALL, "Rus");

	//const int arrSize = 256;
	//const int arrCounts = 102400;

	const int arrSize = 32;
	const int arrCounts = 819200;
	
	int *arrs = new int[arrSize * arrCounts];
	int *arrsGPU = new int[arrSize * arrCounts];
	int *arrsGPUComplex = new int[arrSize * arrCounts];

	// генерация массива
	for (int i = 0; i < arrSize * arrCounts; ++i) {
		arrs[i] = rand() % 10;
		arrsGPU[i] = arrs[i];
	}

	cl_int errcode;

	// печать массива
	//for (int i = 0; i < arrCounts; ++i) {
	//	cout << i << "\t| ";
	//	for (int j = 0; j < arrSize; ++j) {
	//		cout << arrs[i * arrSize + j] << " ";
	//	}
	//	cout << endl;
	//}
	
	// сортировка массива на процессоре(CPU)
	{
		cout << "Сортировка на CPU" << endl;

		auto start = chrono::steady_clock::now();

		// сортировка пузырьком 
		for (int k = 0; k < arrCounts; ++k) {
			for (int i = 0; i < arrSize; ++i) {
				for (int j = 0; j < arrSize - 1 - i; ++j) {
					if (arrs[k * arrSize + j] > arrs[k * arrSize + j + 1]) {
						int temp = arrs[k * arrSize + j];
						arrs[k * arrSize + j] = arrs[k * arrSize + j + 1];
						arrs[k * arrSize + j + 1] = temp;
					}
				}
			}
		}

		auto end = chrono::steady_clock::now();

		// печать массива
		//for (int i = 0; i < arrCounts; ++i) {
		//	cout << i << "\t| ";
		//	for (int j = 0; j < arrSize; ++j) {
		//		cout << arrs[i * arrSize + j] << " ";
		//	}
		//	cout << endl;
		//}

		cout << "Bubble sort CPU: \t\t\t\t"
			<< (double)(chrono::duration_cast<chrono::nanoseconds>(end - start).count()) / 1000000
			<< " ms" << endl;
	}
	 
	// сортировка массива на видеокарте(GPU)(simple)
	{
		cout << "Сортировка на GPU(simple)" << endl;

		cl_platform_id platform;
		cl_uint num_platforms;
		clGetPlatformIDs(1, &platform, &num_platforms);

		cl_device_id device;
		clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

		const cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);

		cl_command_queue_properties props[3] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };

		cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, nullptr);

		const char* source = "__kernel void ArrSortSimple(__global int *inputArr, int size, int count)\n \
		{\n \
			uint k=get_global_id(0);\n \
			for (int i = 0; i < size; ++i) {\n \
				for (int j = 0; j < size - 1 - i; ++j) {\n \
					if (inputArr[k * size + j] > inputArr[k * size + j + 1]) {\n \
						int temp = inputArr[k * size + j];\n \
						inputArr[k * size + j] = inputArr[k * size + j + 1];\n \
						inputArr[k * size + j + 1] = temp;\n \
					}\n \
				}\n \
			}\n \
		};";

		const cl_program program = clCreateProgramWithSource(context, 1, &source, nullptr, nullptr);

		clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

		const cl_kernel kernel = clCreateKernel(program, "ArrSortSimple", nullptr);

		cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arrSize * arrCounts * sizeof(int), arrsGPU, nullptr);

		clSetKernelArg(kernel, 0, sizeof(buffer), (void*)&buffer);
		clSetKernelArg(kernel, 1, sizeof(arrSize), (void*)&arrSize);
		clSetKernelArg(kernel, 2, sizeof(arrCounts), (void*)&arrCounts);

		cl_event event;

		size_t gridSize = arrCounts;
		size_t blockSize = arrSize;

		clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &gridSize, nullptr, 0, nullptr, &event);

		clWaitForEvents(1, &event);
		
		cl_ulong time_start, time_end;
		
		clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr);
		clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr);
		double elapsedTimeGPU = (double)(time_end - time_start)/1e6;
		clReleaseEvent(event);

		clFinish(queue);

		int* resArrs = new int[arrSize * arrCounts];

		clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, arrSize * arrCounts * sizeof(int), resArrs, 0, nullptr, nullptr);

		// печать массива
		//for (int i = 0; i < arrCounts; ++i) {
		//	cout << i << "\t| ";
		//	for (int j = 0; j < arrSize; ++j) {
		//		cout << resArrs[i * arrSize + j] << " ";
		//	}
		//	cout << endl;
		//}

		cout << "Bubble sort GPU(simple): \t\t\t";
		if (!memcmp(arrs, resArrs, arrSize * arrCounts * sizeof(int))) {
			cout << elapsedTimeGPU
				<< " ms" << endl;
		}
		else {
			cout << "fail" << endl;
		}
	}
	 
	// сортировка массива на видеокарте(GPU)(arr rewrite)
	{
		//cout << "Сортировка на GPU(arr rewrite)" << endl;

		//auto starts = chrono::steady_clock::now();

		//for (int i = 0; i < arrCounts * arrSize; ++i) {
		//	arrsGPUComplex[i] = arrsGPU[(i * arrSize) % (arrCounts * arrSize) + i / arrCounts];
		//}

		//auto ends = chrono::steady_clock::now();

		//cl_platform_id platform;
		//cl_uint num_platforms;
		//clGetPlatformIDs(1, &platform, &num_platforms);

		//cl_device_id device;
		//clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

		//const cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);

		//cl_command_queue_properties props[3] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };

		//cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, nullptr);

		//const char* source = "__kernel void ArrSortRewrite(__global int *inputArr, int size, int count)\n \
		//{\n \
		//	uint cur;\n \
		//	uint next;\n \
		//	uint idx;\n \
		//	uint k=get_global_id(0);\n \
		//	for (int i = 0; i < size; ++i) {\n \
		//		idx = size - 1 - i;\n \
		//		for (int j = 0; j < idx; ++j) {\n \
		//			cur = k + j * count;\n \
		//			next = k + (j + 1) * count;\n \
		//			if (inputArr[cur] > inputArr[next]) {\n \
		//				int temp = inputArr[cur];\n \
		//				inputArr[cur] = inputArr[next];\n \
		//				inputArr[next] = temp;\n \
		//			}\n \
		//		}\n \
		//	}\n \
		//};";

		//const cl_program program = clCreateProgramWithSource(context, 1, &source, nullptr, nullptr);

		//clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

		//const cl_kernel kernel = clCreateKernel(program, "ArrSortRewrite", nullptr);

		//cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arrSize * arrCounts * sizeof(int), arrsGPUComplex, nullptr);

		//clSetKernelArg(kernel, 0, sizeof(buffer), (void*)&buffer);
		//clSetKernelArg(kernel, 1, sizeof(arrSize), (void*)&arrSize);
		//clSetKernelArg(kernel, 2, sizeof(arrCounts), (void*)&arrCounts);

		//cl_event event;

		//size_t gridSize = arrCounts;
		//size_t blockSize = arrSize;

		//clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &gridSize, nullptr, 0, nullptr, &event);

		//clWaitForEvents(1, &event);

		//cl_ulong time_start, time_end;

		//clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr);
		//clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr);
		//double elapsedTimeGPU = (double)(time_end - time_start) / 1e6;
		//clReleaseEvent(event);

		//clFinish(queue);

		//int* resArrs = new int[arrSize * arrCounts];

		//clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, arrSize * arrCounts * sizeof(int), resArrs, 0, nullptr, nullptr);

		//for (int i = 0; i < arrCounts * arrSize; ++i) {
		//	arrsGPUComplex[(i * arrSize) % (arrCounts * arrSize) + i / arrCounts] = resArrs[i];
		//}

		//// печать массива
		////for (int i = 0; i < arrCounts; ++i) {
		////	cout << i << "\t| ";
		////	for (int j = 0; j < arrSize; ++j) {
		////		cout << arrsGPUComplex[i * arrSize + j] << " ";
		////	}
		////	cout << endl;
		////}

		//cout << "Bubble sort GPU(arr rewrite): \t\t\t";
		//if (!memcmp(arrs, arrsGPUComplex, arrSize * arrCounts * sizeof(int))) {
		//	cout << elapsedTimeGPU	<< " ms" 
		//		<< " + ( " << (double)(chrono::duration_cast<chrono::nanoseconds>(ends - starts).count()) / 1000000
		//		<< " x2 ms)"
		//		<< endl;
		//}
		//else {
		//	cout << "fail" << endl;
		//}
	}

	{}///todo: глобальной синхронизации на видеокарте не бывает, сделать 3 разных kernel
	// сортировка массива на видеокарте(GPU)(arr rewrite(Kernel))
	{
		cout << "Сортировка на GPU(arr rewrite(Kernel))" << endl;

		cl_platform_id platform;
		cl_uint num_platforms;
		clGetPlatformIDs(1, &platform, &num_platforms);

		cl_device_id device;
		clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

		const cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);

		cl_command_queue_properties props[3] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };

		cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, nullptr);

		const char* ArrSort = "__kernel void ArrSortRewriteKer(__global int *inputArr, int size, int count)\n \
		{\n \
			int cur;\n \
			int next;\n \
			int k=get_global_id(0);\n \
			int idx;\n \
			for (int i = 0; i < size; ++i) {\n \
				idx = size - 1 - i;\n \
				for (int j = 0; j < idx; ++j) {\n \
					cur = inputArr[k + j * count];\n \
					next = inputArr[k + (j + 1) * count];\n \
					if (cur > next) {\n \
						inputArr[k + j * count] = next;\n \
						inputArr[k + (j + 1) * count] = cur;\n \
					}\n \
				}\n \
			}\n \
		};";

		const char* TransposeTo = "__kernel void TransposeTo(__global int *inputArr, __global int *outArr, int size, int count)\n \
		{\n \
			int k=get_global_id(0);\n \
			\n \
			for (int i = 0; i < size; ++i) {\n \
				if (k<count) outArr[k + i * count] = inputArr[k * size + i]; \n \
			}\n \
		};";

		const char* TransposeFrom = "__kernel void TransposeFrom(__global int *inputArr, __global int *outArr, int size, int count)\n \
		{\n \
			int k=get_global_id(0);\n \
			\n \
			for (int i = 0; i < size; ++i) {\n \
				if (k<count) inputArr[k * size + i] = outArr[k + i * count]; \n \
			}\n \
		};";

		const cl_program ArrSortProgram = clCreateProgramWithSource(context, 1, &ArrSort, nullptr, nullptr);
		const cl_program TransposeToProgram = clCreateProgramWithSource(context, 1, &TransposeTo, nullptr, nullptr);
		const cl_program TransposeFromProgram = clCreateProgramWithSource(context, 1, &TransposeFrom, nullptr, nullptr);

		clBuildProgram(ArrSortProgram, 1, &device, nullptr, nullptr, nullptr);
		clBuildProgram(TransposeToProgram, 1, &device, nullptr, nullptr, nullptr);
		clBuildProgram(TransposeFromProgram, 1, &device, nullptr, nullptr, nullptr);

		const cl_kernel ArrSortKernel = clCreateKernel(ArrSortProgram, "ArrSortRewriteKer", nullptr);
		const cl_kernel TransposeToKernel = clCreateKernel(TransposeToProgram, "TransposeTo", nullptr);
		const cl_kernel TransposeFromKernel = clCreateKernel(TransposeFromProgram, "TransposeFrom", nullptr);

		cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arrSize * arrCounts * sizeof(int), arrsGPU, nullptr);
		cl_mem output = clCreateBuffer(context, CL_MEM_READ_WRITE, arrSize * arrCounts * sizeof(int), nullptr, nullptr);

		clSetKernelArg(ArrSortKernel, 0, sizeof(output), (void*)&output);
		clSetKernelArg(ArrSortKernel, 1, sizeof(arrSize), (void*)&arrSize);
		clSetKernelArg(ArrSortKernel, 2, sizeof(arrCounts), (void*)&arrCounts);

		clSetKernelArg(TransposeToKernel, 0, sizeof(buffer), (void*)&buffer);
		clSetKernelArg(TransposeToKernel, 1, sizeof(output), (void*)&output);
		clSetKernelArg(TransposeToKernel, 2, sizeof(arrSize), (void*)&arrSize);
		clSetKernelArg(TransposeToKernel, 3, sizeof(arrCounts), (void*)&arrCounts);

		clSetKernelArg(TransposeFromKernel, 0, sizeof(buffer), (void*)&buffer);
		clSetKernelArg(TransposeFromKernel, 1, sizeof(output), (void*)&output);
		clSetKernelArg(TransposeFromKernel, 2, sizeof(arrSize), (void*)&arrSize);
		clSetKernelArg(TransposeFromKernel, 3, sizeof(arrCounts), (void*)&arrCounts);

		cl_event event;
		cl_event TransposeToEvent;
		cl_event TransposeFromEvent;

		size_t gridSize = arrCounts;
		cl_long time_start, time_end;

		clEnqueueNDRangeKernel(queue, TransposeToKernel, 1, nullptr, &gridSize, nullptr, 0, nullptr, &TransposeToEvent);
		checkErrorEx(errcode = clWaitForEvents(1, &TransposeToEvent));
		checkErrorEx(errcode = clGetEventProfilingInfo(TransposeToEvent, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr));
		checkErrorEx(errcode = clGetEventProfilingInfo(TransposeToEvent, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr));
		double elapsedTimeGPUTrTo = (double)(time_end - time_start) / 1e6;
		clReleaseEvent(TransposeToEvent);

		clEnqueueNDRangeKernel(queue, ArrSortKernel, 1, nullptr, &gridSize, nullptr, 0, nullptr, &event);
		checkErrorEx(errcode = clWaitForEvents(1, &event));
		checkErrorEx(errcode = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr));
		checkErrorEx(errcode = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr));
		double elapsedTimeGPUSort = (double)(time_end - time_start) / 1e6;
		clReleaseEvent(event);

		clEnqueueNDRangeKernel(queue, TransposeFromKernel, 1, nullptr, &gridSize, nullptr, 0, nullptr, &TransposeFromEvent);
		checkErrorEx(errcode = clWaitForEvents(1, &TransposeFromEvent));
		checkErrorEx(errcode = clGetEventProfilingInfo(TransposeFromEvent, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr));
		checkErrorEx(errcode = clGetEventProfilingInfo(TransposeFromEvent, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr));
		double elapsedTimeGPUTrFrom = (double)(time_end - time_start) / 1e6;
		clReleaseEvent(TransposeFromEvent);

		clFinish(queue);

		int* resArrs = new int[arrSize * arrCounts];

		checkErrorEx(errcode = clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, arrSize * arrCounts * sizeof(int), resArrs, 0, nullptr, nullptr));

		// печать массива
		//for (int i = 0; i < arrCounts; ++i) {
		//	cout << i << "\t| ";
		//	for (int j = 0; j < arrSize; ++j) {
		//		cout << resArrs[i * arrSize + j] << " ";
		//	}
		//	cout << endl;
		//}

		cout << "Bubble sort GPU(arr rewrite(Kernel)): \t\t";
		if (!memcmp(arrs, resArrs, arrSize * arrCounts * sizeof(int))) {
			cout << elapsedTimeGPUSort << " ms"
				<< " + ( " << elapsedTimeGPUTrTo
				<< " + " << elapsedTimeGPUTrFrom
				<< " ms )"
				<< endl;
		}
		else {
			cout << "fail " << endl;
		}
	}

	{}/// todo: сделать такой же rewrite но с использованием local(shared) memory (а может не надо?)
	// сортировка массива на видеокарте(GPU)(arr rewrite(Kernel + local memory))
	{
		//cout << "Сортировка на GPU(arr rewrite(Kernel + local memory))" << endl;

		//cl_platform_id platform;
		//cl_uint num_platforms;
		//clGetPlatformIDs(1, &platform, &num_platforms);

		//cl_device_id device;
		//clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

		//const cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);

		//cl_command_queue_properties props[3] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };

		//cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, nullptr);

		//const char* source = "__kernel void ArrSortRewriteKerMem(__global int *inputArr, int size, int count)\n \
		//{\n \
		//	uint cur;\n \
		//	uint next;\n \
		//	ulong idx;\n \
		//	ulong ind; \n \
		//	\n \
		//	uint gi = get_global_id(0); \n \
		//	uint li = get_local_id(0); \n \
		//	\n \
		//	__local int localBuff[8192];\n \
		//	\n \
		//	for (int i = 0; i < size; ++i) {\n \
		//		idx = gi + i * count;\n \
		//		localBuff[idx] = inputArr[((gi + i * count) % count) * size + (idx*2) / (count*2)]; \n \
		//	}\n \
		//	barrier(CLK_LOCAL_MEM_FENCE);\n \
		//	for (int i = 0; i < size; ++i) {\n \
		//		idx = size - 1 - i;\n \
		//		for (int j = 0; j < idx; ++j) {\n \
		//			cur = k + j * count;\n \
		//			next = k + (j + 1) * count;\n \
		//			if (outArr[cur] > outArr[next]) {\n \
		//				int temp = outArr[cur];\n \
		//				outArr[cur] = outArr[next];\n \
		//				outArr[next] = temp;\n \
		//			}\n \
		//		}\n \
		//	}\n \
		//	barrier(CLK_LOCAL_MEM_FENCE);\n \
		//	for (int i = 0; i < size; ++i) {\n \
		//		idx = k + i * count;\n \
		//		inputArr[((k + i * count) % count) * size + (idx*2) / (count*2)] = outArr[idx]; \n \
		//	}\n \
		//};";
		//const cl_program program = clCreateProgramWithSource(context, 1, &source, nullptr, nullptr);

		//clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

		//const cl_kernel kernel = clCreateKernel(program, "ArrSortRewriteKerMem", nullptr);

		//cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arrSize * arrCounts * sizeof(int), arrsGPU, nullptr);

		//clSetKernelArg(kernel, 0, sizeof(buffer), (void*)&buffer);
		//clSetKernelArg(kernel, 1, sizeof(arrSize), (void*)&arrSize);
		//clSetKernelArg(kernel, 2, sizeof(arrCounts), (void*)&arrCounts);

		//cl_event event;

		//size_t gridSize = arrCounts;
		//size_t blockSize = 256;

		//clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &gridSize, &blockSize, 0, nullptr, &event);

		//clWaitForEvents(1, &event);

		//cl_ulong time_start, time_end;

		//clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr);
		//clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr);
		//double elapsedTimeGPU = (double)(time_end - time_start) / 1e6;
		//clReleaseEvent(event);

		//clFinish(queue);

		//int* resArrs = new int[arrSize * arrCounts];

		//clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, arrSize * arrCounts * sizeof(int), resArrs, 0, nullptr, nullptr);

		//// печать массива
		////for (int i = 0; i < arrCounts; ++i) {
		////	cout << i << "\t| ";
		////	for (int j = 0; j < arrSize; ++j) {
		////		cout << resArrs[i * arrSize + j] << " ";
		////	}
		////	cout << endl;
		////}

		//cout << "Bubble sort GPU(arr rewrite(Kernel + local memory)): \t";
		//if (!memcmp(arrs, resArrs, arrSize * arrCounts * sizeof(int))) {
		//	cout << elapsedTimeGPU
		//		<< " ms" << endl;
		//}
		//else {
		//	cout << "fail " << endl;
		//}
	}

	{}
	// сортировка массива на видеокарте(GPU)(local(shared) memory)
	{
		cout << "Сортировка на GPU(local(shared) memory)" << endl;

		cl_platform_id platform;
		cl_uint num_platforms;
		clGetPlatformIDs(1, &platform, &num_platforms);

		cl_device_id device;
		clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

		const cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);

		cl_command_queue_properties props[3] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };

		cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, nullptr);

		const char* source = "__kernel void ArrSortLocMem(__global int *inputArr, int size, int count)\n \
		{\n \
			uint cur;\n \
			uint next;\n \
			uint idx;\n \
			uint gi=get_global_id(0);\n \
			uint li=get_local_id(0);\n \
			\n \
			__local int localBuff[256][32+1];\n \
			//__local int localBuff[8192];\n \
			for (int i = 0; i < 32; ++i){\n \
				idx = li + 256 * i;\n \
				localBuff[idx/32][idx%32] = inputArr[8192 * (gi / 256) + idx];\n \
			}\n \
			barrier(CLK_LOCAL_MEM_FENCE);\n \
			\n \
			for (int i = 0; i < size; ++i) {\n \
				idx = size - 1 - i;\n \
				for (int j = 0; j < idx; ++j) {\n \
					cur = localBuff[li][j];\n \
					next = localBuff[li][j+1];\n \
					if (cur > next) {\n \
						localBuff[li][j] = next;\n \
						localBuff[li][j+1] = cur;\n \
					}\n \
				}\n \
			}\n \
			barrier(CLK_LOCAL_MEM_FENCE);\n \
			for (int i = 0; i < 32; ++i){\n \
				idx = li + 256 * i;\n \
				inputArr[8192 * (gi / 256) + idx] = localBuff[idx/32][idx%32];\n \
			}\n \
		};";

		const cl_program program = clCreateProgramWithSource(context, 1, &source, nullptr, nullptr);

		clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

		const cl_kernel kernel = clCreateKernel(program, "ArrSortLocMem", nullptr);

		cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arrSize * arrCounts * sizeof(int), arrsGPU, nullptr);

		clSetKernelArg(kernel, 0, sizeof(buffer), (void*)&buffer);
		clSetKernelArg(kernel, 1, sizeof(arrSize), (void*)&arrSize);
		clSetKernelArg(kernel, 2, sizeof(arrCounts), (void*)&arrCounts);

		cl_event event;

		size_t gridSize = arrCounts;
		size_t blockSize = 256;

		clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &gridSize, &blockSize, 0, nullptr, &event);

		clWaitForEvents(1, &event);

		cl_ulong time_start, time_end;

		clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr);
		clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr);
		double elapsedTimeGPU = (double)(time_end - time_start) / 1e6;
		clReleaseEvent(event);

		clFinish(queue);

		int* resArrs = new int[arrSize * arrCounts];

		clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, arrSize* arrCounts * sizeof(int), resArrs, 0, nullptr, nullptr);

		// печать массива
		//for (int i = 0; i < arrCounts; ++i) {
		//	cout << i << "\t| ";
		//	for (int j = 0; j < arrSize; ++j) {
		//		cout << resArrs[i * arrSize + j] << " ";
		//	}
		//	cout << endl;
		//}

		cout << "Bubble sort GPU(local(shared) memory): \t\t";
		if (!memcmp(arrs, resArrs, arrSize * arrCounts * sizeof(int))) {
			cout << elapsedTimeGPU
				<< " ms" << endl;
		}
		else {
			cout << "fail" << endl;
		}
	}

	{}
	// сортировка массива на видеокарте(GPU)(local(shared) memory(cycled))
	{
		//cout << "Сортировка на GPU(local(shared) memory(cycled))" << endl;

		//cl_platform_id platform;
		//cl_uint num_platforms;
		//clGetPlatformIDs(1, &platform, &num_platforms);

		//cl_device_id device;
		//clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

		//const cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);

		//cl_command_queue_properties props[3] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };

		//cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, nullptr);

		//const char* source = "__kernel void ArrSortLocMem(__global int *inputArr, int size, int count)\n \
		//{\n \
		//	uint cur;\n \
		//	uint next;\n \
		//	uint idx;\n \
		//	uint gi=get_global_id(0);\n \
		//	uint li=get_local_id(0);\n \
		//	\n \
		//	__local int localBuff[8192];\n \
		//	for (int el = 0; el < count/256; ++el){\n \
		//		for (int i = 0; i < 32; ++i){\n \
		//			idx = li + 256 * i;\n \
		//			localBuff[idx] = inputArr[idx + el*8192];\n \
		//		}\n \
		//		barrier(CLK_LOCAL_MEM_FENCE);\n \
		//		\n \
		//		for (int i = 0; i < size; ++i) {\n \
		//			idx = size - 1 - i;\n \
		//			for (int j = 0; j < idx; ++j) {\n \
		//				cur = li * size + j;\n \
		//				next = li * size + j + 1;\n \
		//				if (localBuff[cur] > localBuff[next]) {\n \
		//					int temp = localBuff[cur];\n \
		//					localBuff[cur] = localBuff[next];\n \
		//					localBuff[next] = temp;\n \
		//				}\n \
		//			}\n \
		//		}\n \
		//		barrier(CLK_LOCAL_MEM_FENCE);\n \
		//		for (int i = 0; i < 32; ++i){\n \
		//			idx = li + 256 * i;\n \
		//			inputArr[idx + el*8192] = localBuff[idx];\n \
		//		}\n \
		//	}\n \
		//};";

		//const cl_program program = clCreateProgramWithSource(context, 1, &source, nullptr, nullptr);

		//clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

		//const cl_kernel kernel = clCreateKernel(program, "ArrSortLocMem", nullptr);

		//cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arrSize * arrCounts * sizeof(int), arrsGPU, nullptr);

		//clSetKernelArg(kernel, 0, sizeof(buffer), (void*)&buffer);
		//clSetKernelArg(kernel, 1, sizeof(arrSize), (void*)&arrSize);
		//clSetKernelArg(kernel, 2, sizeof(arrCounts), (void*)&arrCounts);

		//cl_event event;

		//size_t gridSize = 256;
		//size_t blockSize = 256;

		//clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &gridSize, &blockSize, 0, nullptr, &event);

		//clWaitForEvents(1, &event);

		//cl_ulong time_start, time_end;

		//clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, nullptr);
		//clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, nullptr);
		//double elapsedTimeGPU = (double)(time_end - time_start) / 1e6;
		//clReleaseEvent(event);

		//clFinish(queue);

		//int* resArrs = new int[arrSize * arrCounts];

		//clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, arrSize * arrCounts * sizeof(int), resArrs, 0, nullptr, nullptr);

		//// печать массива
		////for (int i = 0; i < arrCounts; ++i) {
		////	cout << i << "\t| ";
		////	for (int j = 0; j < arrSize; ++j) {
		////		cout << resArrs[i * arrSize + j] << " ";
		////	}
		////	cout << endl;
		////}

		//cout << "Bubble sort GPU(local(shared) memory(cycled)): \t";
		//if (!memcmp(arrs, resArrs, arrSize * arrCounts * sizeof(int))) {
		//	cout << elapsedTimeGPU
		//		<< " ms" << endl;
		//}
		//else {
		//	cout << "fail" << endl;
		//}
	}

	return 0;
}