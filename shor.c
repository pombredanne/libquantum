/* shor.c: implementation of Shor's quantum algorithm for factorization
 */

#include "quantum_stdlib.h"
#include "quantum_reg.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "shor.h"

/* TODO: Change input parsing and/or accepted parameters
 * Currently mirrors libquantum's formatting exactly to allow correctness testing.
 * This technically makes our code licensed under the GPL v2 unless removed.
 */
int main(int argc, char** argv) {
	if(argc == 1) {
		printf("Usage: sim [number]\n\n");
		return 3;
	}

	int N = atoi(argv[1]);

	if(N < 15) {
		printf("Invalid number\n\n");
		return 3;
	}

	// Find a number x relatively prime to n
	srand(time(NULL));
	int x;
	do {
		x = rand() % N;
	} while(quda_gcd_div(N,x) > 1 || x < 2);

	x = 8; // DEBUG
	printf("Random seed: %i\n", x);

	int L = qubits_required(N);
	int width = qubits_required(N*N);
	//int width = 2*L+2;

	printf("N = %i, %i qubits required\n", N, width+L);

	quantum_reg qr1;
	quda_quantum_reg_init(&qr1,width);
	quda_quantum_reg_set(&qr1,0);

	int err;
	/* DEBUG */
	err = quda_check_normalization(&qr1);
	err |= quda_weak_check_amplitudes(&qr1);
	if(err != 0) {
		printf("ERROR: ");
	}
	printf("After quda_quantum_reg_set()\n");
	/* END DEBUG */

	quda_quantum_hadamard_all(&qr1);

	/* DEBUG */
	err = quda_check_normalization(&qr1);
	err |= quda_weak_check_amplitudes(&qr1);
	if(err != 0) {
		printf("ERROR: ");
	}
	printf("After quda_quantum_hadamard_all()\n");
	/* END DEBUG */

	quda_quantum_add_scratch(3*L+2,&qr1); // Full scratch space not needed for classical algorithm
	//quda_quantum_add_scratch(L,&qr1); // effectively creates 'output' subregister for exp_mod_n()
	quda_classical_exp_mod_n(x,N,&qr1);

	/* DEBUG */
	err = quda_check_normalization(&qr1);
	err |= quda_weak_check_amplitudes(&qr1);
	if(err != 0) {
		printf("ERROR: ");
	}
	printf("After quda_classical_exp_mod_n()\n");
	printf("Ok going into quantum_collapse_scratch()\n");
	/* END DEBUG */

	/* libquantum measures all of its scratch bits here for some reason.
	 * Presumedly, this reduces memory overhead by finding the single most likely value of
	 * x^(hadamarded-input) % n. Thus their qreg would be left with only inputs that correspond
	 * to the most likely output. It may also be a correctness issue since they use the least
	 * significant register bits to store scratch (with no differentiation from regular bits).
	 * Comment the next line for the opposite effect (no coalescing).
	 */
	//quda_quantum_collapse_scratch(&qr1);
	quda_quantum_clear_scratch(&qr1);

	/* DEBUG */
	err = quda_check_normalization(&qr1);
	err |= quda_weak_check_amplitudes(&qr1);
	if(err != 0) {
		printf("ERROR: ");
	}
	printf("After quantum_collapse_scratch()\n");
	printf("Going into quantum_fourier_transform()\n");
	//quda_quantum_reg_dump(&qr1,"BEFORE_FOURIER");
	/* END DEBUG */

	quda_quantum_fourier_transform(&qr1);

	/* DEBUG */
	err = quda_check_normalization(&qr1);
	err |= quda_weak_check_amplitudes(&qr1);
	if(err != 0) {
		printf("ERROR: ");
	}
	printf("After quda_quantum_fourier_transform()\n");
	printf("Going into reg_measure_and_collapse\n");
	//quda_quantum_reg_dump(&qr1,"AFTER_FOURIER");
	/* END DEBUG */

	uint64_t result;
	int res = quda_quantum_reg_measure_and_collapse(&qr1,&result);
	if(res == -1) {
		printf("Invalid result (normalization error).\n");
		return -1;
	} else if(result == 0) {
		printf("Measured zero.\n");
		return 0;
	}

	uint64_t denom = 1 << width;
	quda_classical_continued_fraction_expansion(&result,&denom);

	printf("fractional approximation is %lu/%lu.\n", result, denom);

	if((denom % 2 == 1) && (2*denom < (1<<width))) {
		printf("Odd denominator, trying to expand by 2.\n");
		denom *= 2;
    }

	if(denom % 2 == 1) {
		printf("Odd period, try again.\n");
		return 0;
	}

	printf("Possible period is %lu.\n", denom);

	int factor = pow(x,denom/2);
	int factor1 = quda_gcd_div(N,factor + 1);
	int factor2 = quda_gcd_div(N,factor - 1);
	if(factor1 > factor2) {
		factor = factor1;
	} else {
		factor = factor2;
	}

	if(factor < N && factor > 1) {
		printf("%d = %d * %d\n",N,factor,N/factor);
	} else {
		printf("Could not determine factors.\n");
	}

	quda_quantum_reg_delete(&qr1);

	return 0;
}

// Classical functions

int qubits_required(int num) {
	int j = 1;
	int i;
	for(i=1;j < num;i++) {
		j <<= 1;
	}
	return i;
}
