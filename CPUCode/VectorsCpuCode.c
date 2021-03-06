#include <stdio.h>
#include <stdlib.h>
#include "Maxfiles.h"
#include <MaxSLiCInterface.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <complex.h>

// 1. double
// 2. float
// 3. int
typedef float dataType;
typedef float complex cplx;

//fft implementation taken from https://rosettacode.org/wiki/Fast_Fourier_transform#C
void _fft(cplx * buf, cplx * out, int n, int step) {
	if (step < n) {
		_fft(out, buf, n, step * 2);
		_fft(out + step, buf + step, n, step * 2);
 
		for (int i = 0; i < n; i += 2 * step) {
			cplx t = cexp(-I * M_PI * i / n) * out[i + step];
			buf[i / 2]     = out[i] + t;
			buf[(i + n)/2] = out[i] - t;
		}
	}
}
 
void fft(cplx * buf, int n) {
	cplx out[n];
	for (int i = 0; i < n; i++) out[i] = buf[i];
 
	_fft(buf, out, n, 1);
}

void initialize(int numberOfVectors, dataType *inVector1, dataType *inVector2) {

	srand(time(NULL));
	int min=-10;
	int max=+10;

	int nonZeroVec1;

	for (int k = 0; k < numberOfVectors; k++) {
		nonZeroVec1 = rand() % Vectors_vectorSize + 1;

		for (int i=0; i < nonZeroVec1; i++){
			inVector1[k*Vectors_vectorSize+i] = min + (rand() / (dataType) RAND_MAX) * ( max - min );
		}

		for (int j=0; j < (Vectors_vectorSize-nonZeroVec1+1); j++){
			inVector2[k*Vectors_vectorSize+j] = min + (rand() / (dataType) RAND_MAX) * ( max - min );
		}
	}
}

int check(int numberOfVectors, dataType *outVector, dataType *expectedVector) {
	int status = 0;
	float eps = 1.192093e-07;
	for (int i = 0; i < numberOfVectors * Vectors_vectorSize; i++) {
		if (abs(outVector[i] - expectedVector[i]) > eps) {
			//printf("%f != %f \n", outVector[i], expectedVector[i]);
			status = 1;
			break;
		}
	}
	return status;
}

void VectorsCPU(int numberOfVectors, dataType *inVector1, dataType *inVector2, dataType *outVector1) {

	cplx * fftOut1 = calloc(Vectors_vectorSize * numberOfVectors, sizeof(cplx));
	cplx * fftOut2 = calloc(Vectors_vectorSize * numberOfVectors, sizeof(cplx));
	cplx * fftMulti = calloc(Vectors_vectorSize * numberOfVectors, sizeof(cplx));

	for (int i = 0; i < Vectors_vectorSize * numberOfVectors; i++) {
		fftOut1[i] = inVector1[i] + 0.0 * I;
		fftOut2[i] = inVector2[i] + 0.0 * I;
	}

	for (int k = 0; k < numberOfVectors; k++) {
		//Step 1 - FFT both signals
		fft(&fftOut1[k*Vectors_vectorSize], Vectors_vectorSize);
		fft(&fftOut2[k*Vectors_vectorSize], Vectors_vectorSize);	
		
		//Step 2 - Multiplying ffts of signals
		for (int i = 0; i < Vectors_vectorSize; i++) {
			fftMulti[k*Vectors_vectorSize+i] = fftOut1[k*Vectors_vectorSize+i] * fftOut2[k*Vectors_vectorSize+i];

			//Step 3 - swapping imag and real part
			cplx buf = fftMulti[k*Vectors_vectorSize+i];
			fftMulti[k*Vectors_vectorSize+i] = cimag(buf) + creal(buf)*I;			
		}	

		//Step 4 - FFT (IFFT) multiplied signals
		fft(&fftMulti[k*Vectors_vectorSize], Vectors_vectorSize);	

		//Step 5 - normalising output signal + swapping
		for (int j = 0; j < Vectors_vectorSize; j++) {
			outVector1[k*Vectors_vectorSize+j] = cimag(fftMulti[k*Vectors_vectorSize+j])/Vectors_vectorSize;
		}
	}

	free(fftOut1);
	free(fftOut2);
	free(fftMulti);
}

int convertArgToInt(char * str) {
	if (strcmp ("-num", str) == 0) return 1;
	else if (strcmp ("-check", str) == 0) return 2;
	else if (strcmp ("-print", str) == 0) return 3;
	else if (strcmp ("-export", str) == 0) return 4;
	else return 0;
}

int main(int argc, char *argv[]) {
	clock_t begin,end;
	double time_spent, time_spentCPU;

	printf("=================\n");
	printf("Addition stream\n");
	printf("Precision: %ld\n",sizeof(dataType)*8);
	printf("Matrix size: %d\n",Vectors_vectorSize);
	printf("=================\n");
	
	int numberOfVectors = 2;
	size_t boolTest = 0, print = 0, export = 0;

	//commandline parameter parsing
	for (int i=1; i<argc; i++) {
		switch( convertArgToInt(argv[i]) ) {
			case 1: numberOfVectors = atoi(argv[++i]); break;
			case 2: boolTest = 1; break;
			case 3: print = 1; break;
			case 4: export = 1; break;
			default: break;
		}
	}

	size_t sizeBytes = Vectors_vectorSize * numberOfVectors * sizeof(dataType);
	dataType *inVector1= calloc(Vectors_vectorSize * numberOfVectors, sizeof(dataType));
	dataType *inVector2= calloc(Vectors_vectorSize * numberOfVectors, sizeof(dataType));
	dataType *outVector = malloc(sizeBytes);
	dataType *expectedVector = malloc(sizeBytes);

	printf("Initializing...\n");
	initialize(numberOfVectors, inVector1, inVector2);

	printf("Running CPU.\n");
	begin = clock();
	VectorsCPU(numberOfVectors, inVector1, inVector2, expectedVector);
	end = clock();
	time_spentCPU = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("Time CPU %d iterations: %lf\n", numberOfVectors, time_spentCPU);

	printf("Running DFE.\n");
	begin = clock();
	Vectors(numberOfVectors, inVector1, inVector2, outVector);
	end = clock();
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("Time DFE %d iterations: %lf\n", numberOfVectors, time_spent);

	if (boolTest) {
		printf("Verifying...\n");
		int status = check(numberOfVectors, outVector, expectedVector);
		if (status) printf("Test failed.\n");
		else printf("Test passed OK!\n");
	}

	//Exporting benchmark results
	if (export) {
		FILE* time_res;
		char filename[32];
		printf("Exporting to file...\n");
		snprintf(filename, sizeof(filename), "benchmark.csv");
		time_res = fopen(filename,"a");
		fprintf(time_res, "%d, %d, %f, %f\n", Vectors_vectorSize, numberOfVectors, time_spentCPU, time_spent);
		fclose(time_res);
	}

	if (print) {
		for (int k = 0; k < numberOfVectors; k++) {
			printf("========== Vector set %d ===========\n", k);
			printf("in1: ");
			for (int i=0; i < Vectors_vectorSize; i++){
				printf("%.3f ", inVector1[k*Vectors_vectorSize+i]);
			}

			printf("\nin2: ");
			for (int j=0; j < Vectors_vectorSize; j++){
				printf("%.3f ", inVector2[k*Vectors_vectorSize+j]);
			}

			printf("\nout: ");
			for (int l=0; l < Vectors_vectorSize; l++){
				printf("%.3f ", outVector[k*Vectors_vectorSize+l]);
			}

			printf("\ncpu: ");
			for (int l=0; l < Vectors_vectorSize; l++){
				printf("%.3f ", expectedVector[k*Vectors_vectorSize+l]);
			}

			printf("\n===================================\n");
		}
	}

	free(inVector1);
	free(inVector2);
	free(outVector);
	free(expectedVector);
	
	return 0;
}
