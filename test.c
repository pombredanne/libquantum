/* test.c: simple testing program for base libquantum functionality
*/

#include <math.h>
#include <stdio.h>
#include "complex.h"
#include "quantum_reg.h"
#include "quantum_gates.h"

#define CHECK_COMPLEX_RESULT(val, compreal, compimag, explain) \
  do { \
  if (fabs((val).real - compreal) > 1e-3 || fabs((val).imag - compimag) > 1e-3) \
    printf("FAIL TEST " explain ": got " \
      "%.3f + %.3fi, expected %.3f + %.3fi\n", \
      (double)(val).real, (double)(val).imag, (double)compreal, (double)compimag); \
  else \
    printf("PASS TEST " explain "\n"); \
  } while(0)


#define INVOKE1 0
#define INVOKE2 0, 1
#define INVOKE3 0, 1, 2
#define TEST_GATE(nbits, func, ...) \
do { \
  static complex_t matrix[2 * nbits][2 * nbits] = __VA_ARGS__; \
  quantum_reg qureg; \
  quda_quantum_reg_init(&qureg, 1); \
  /* How does it map basis elements? */ \
  for (int i = 0; i < (1 << nbits); i++) { \
    quda_quantum_reg_set(&qureg, i); \
    func(INVOKE##nbits, &qureg); \
    printf("Testing " #func " with basis |%d>\n", i); \
    /* Check all of the states to see if they are correct */ \
    unsigned verified = (1U << (1U << nbits)) - 1; \
    float total_probability = 0; \
    for (int s = 0; s < qureg.num_states; s++) { \
      complex_t *amplitude = &qureg.states[s].amplitude; \
      int state = qureg.states[s].state; \
      if ((verified & (1U << state)) == 0) { \
        printf("FAIL: state %x seen multiple times\n", state); \
      } \
      verified &= ~(1U << state); \
      complex_t *entry = &matrix[i][state]; \
      CHECK_COMPLEX_RESULT(*amplitude, entry->real, entry->imag, \
        "Verifying gate " #func); \
      total_probability += quda_complex_abs_square(*amplitude); \
    } \
    if (fabs(total_probability - 1) > 1e-3) { \
      printf("FAIL TEST verifying gate " #func ": Saw only %d (unseen: %x)\n", \
        qureg.num_states, verified); \
      printf("FAIL TEST verifying gate " #func ": total probability: %.3f\n", \
        (double)total_probability); \
    } \
  } \
  quda_quantum_reg_delete(&qureg); \
} while (0)

int main(int argc, char** argv) {
	// Complex
	complex_t op1,op2;
	op1.real = 1;
	op1.imag = 1;
	op2.real = 2;
	op2.imag = -0.5;
	complex_t res = quda_complex_add(op1,op2);
  CHECK_COMPLEX_RESULT(res, 3, 0.5, "Simple complex addition");

  // Courtesy of jsmath.cpp
#define M_SQRT1_2 0.70710678118654752440f
  // 1-qubit gates
  TEST_GATE(1, quda_quantum_pauli_x_gate, {{{0,0},{1,0}},{{1,0},{0,0}}});
  TEST_GATE(1, quda_quantum_pauli_y_gate, {{{0,0},{0,-1}},{{0,1},{0,0}}});
  TEST_GATE(1, quda_quantum_pauli_z_gate, {{{1,0},{0,0}},{{0,0},{-1,0}}});
  TEST_GATE(1, quda_quantum_hadamard_gate,
    {{{M_SQRT1_2,0},{M_SQRT1_2,0}},{{M_SQRT1_2,0},{-M_SQRT1_2,0}}});
  TEST_GATE(1, quda_quantum_phase_gate, {{{1,0},{0,0}},{{0,0},{0,1}}});
  TEST_GATE(1, quda_quantum_pi_over_8_gate,
      {{{1,0},{0,0}},{{0,0},{M_SQRT1_2,M_SQRT1_2}}});

  // 2-qubit gates
  TEST_GATE(2, quda_quantum_swap_gate,
    {{{1,0},{0,0},{0,0},{0,0}},
     {{0,0},{0,0},{1,0},{0,0}},
     {{0,0},{1,0},{0,0},{0,0}},
     {{0,0},{0,0},{0,0},{1,0}}});
  TEST_GATE(2, quda_quantum_controlled_not_gate,
    {{{1,0},{0,0},{0,0},{0,0}},
     {{0,0},{1,0},{0,0},{0,0}},
     {{0,0},{0,0},{0,0},{1,0}},
     {{0,0},{0,0},{1,0},{0,0}}});
	// Quantum Register
	quantum_reg qreg;
	if(quda_quantum_reg_init(&qreg,16) == -1) return -1;

	quda_quantum_reg_set(&qreg,42);
	if(quda_quantum_reg_enlarge(&qreg,NULL) == -1) return -1;

	quda_quantum_reg_trim(&qreg);
	quda_quantum_reg_delete(&qreg);

  return 0;
}
