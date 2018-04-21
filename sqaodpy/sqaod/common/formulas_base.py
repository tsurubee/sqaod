from __future__ import print_function
import numpy as np
import numbers
import sqaod
from sqaod.common import checkers
import sqaod.common as common

# dense graph    

# QUBO energy functions

def dense_graph_calculate_E(W, x, cext, dtype) :
    checkers.dense_graph.qubo(W)
    checkers.dense_graph.bits(W, x)
    checkers.assert_is_vector('x', x)
    W = common.fix_type(W, dtype)
    x = common.fix_type(x, np.int8)

    E = np.ndarray((1), dtype)
    cext.dense_graph_calculate_E(E, W, x, dtype)
    return E[0]

def dense_graph_batch_calculate_E(W, x, cext, dtype) :
    checkers.dense_graph.qubo(W);
    checkers.dense_graph.bits(W, x)
    W = common.fix_type(W, dtype)
    x = common.fix_type(x, np.int8)
    x = x.reshape(1, -1) if len(x.shape) == 1 else x
    
    E = np.empty((x.shape[0]), dtype)
    cext.dense_graph_batch_calculate_E(E, W, x, dtype)
    return E


# QUBO -> Ising model

def dense_graph_calculate_hamiltonian(W, cext, dtype) :
    W = common.fix_type(W, dtype)
    checkers.dense_graph.qubo(W)

    N = W.shape[0]
    h = np.empty((N), dtype)
    J = np.empty((N, N), dtype)
    c = np.empty((1), dtype)
    cext.dense_graph_calculate_hamiltonian(h, J, c, W, dtype);
    return h, J, c[0]

# Ising model energy functions

def dense_graph_calculate_E_from_spin(h, J, c, q, cext, dtype) :
    checkers.dense_graph.hJc(h, J, c);
    checkers.dense_graph.bits(J, q);
    checkers.assert_is_vector('q', q)
    h, J = common.fix_type([h, J], dtype)
    q = common.fix_type(q, np.int8)
    
    E = np.ndarray((1), dtype)
    cext.dense_graph_calculate_E_from_spin(E, h, J, c, q, dtype)
    return E[0]

def dense_graph_batch_calculate_E_from_spin(h, J, c, q, cext, dtype) :
    h, J = common.fix_type([h, J], dtype)
    checkers.dense_graph.hJc(h, J, c);
    checkers.dense_graph.bits(J, q);

    E = np.empty([q.shape[0]], dtype)
    cext.dense_graph_batch_calculate_E_from_spin(E, h, J, c, q, dtype)
    return E


# bipartite_graph

def bipartite_graph_calculate_E(b0, b1, W, x0, x1, cext, dtype) :
    checkers.bipartite_graph.qubo(b0, b1, W)
    checkers.bipartite_graph.bits(W, x0, x1)
    checkers.assert_is_vector('x0', x0)
    checkers.assert_is_vector('x1', x1)
    
    b0, b1, W = common.fix_type([b0, b1, W], dtype)
    x0, x1 = common.fix_type([x0, x1], np.int8)
    E = np.ndarray((1), dtype)
    cext.bipartite_graph_calculate_E(E, b0, b1, W, x0, x1, dtype)
    return E[0]

def bipartite_graph_batch_calculate_E(b0, b1, W, x0, x1, cext, dtype) :
    checkers.bipartite_graph.qubo(b0, b1, W)
    checkers.bipartite_graph.bits(W, x0, x1)
    b0, b1, W = common.fix_type([b0, b1, W], dtype)
    nBatch0 = 1 if len(x0.shape) == 1 else x0.shape[0]
    nBatch1 = 1 if len(x1.shape) == 1 else x1.shape[0]
    if nBatch0 != nBatch1 :
        raise Exception("Different batch dims between x0 and x1.")
    
    if nBatch0 == 1 :
        x0, x1 = x0.reshape(1, -1), x1.reshape(1, -1)
    E = np.empty((nBatch0), dtype)
    cext.bipartite_graph_batch_calculate_E(E, b0, b1, W, x0, x1, dtype)
    return E

def bipartite_graph_batch_calculate_E_2d(b0, b1, W, x0, x1, cext, dtype) :
    checkers.bipartite_graph.qubo(b0, b1, W)
    checkers.bipartite_graph.bits(W, x0, x1)
    b0, b1, W = common.fix_type([b0, b1, W], dtype)
    x0, x1 = common.fix_type([x0, x1], np.int8)
    
    nBatch0 = 1 if len(x0.shape) == 1 else x0.shape[0]
    nBatch1 = 1 if len(x1.shape) == 1 else x1.shape[0]
    E = np.empty((nBatch1, nBatch0), dtype)
    cext.bipartite_graph_batch_calculate_E_2d(E, b0, b1, W, x0, x1, dtype)
    return E


def bipartite_graph_calculate_hamiltonian(b0, b1, W, cext, dtype) :
    checkers.bipartite_graph.qubo(b0, b1, W)
    b0, b1, W = common.fix_type([b0, b1, W], dtype)

    N0 = W.shape[1]
    N1 = W.shape[0]
    h0 = np.empty((N0), dtype)
    h1 = np.empty((N1), dtype)
    J = np.empty((N1, N0), dtype)
    c = np.empty((1), dtype)
    cext.bipartite_graph_calculate_hamiltonian(h0, h1, J, c, b0, b1, W, dtype);
    return h0, h1, J, c[0]

def bipartite_graph_calculate_E_from_spin(h0, h1, J, c, q0, q1, cext, dtype) :
    checkers.bipartite_graph.hJc(h0, h1, J, c)
    checkers.bipartite_graph.bits(J, q0, q1)
    checkers.assert_is_vector('q0', q0)
    checkers.assert_is_vector('q1', q1)
    h0, h1, W = common.fix_type([h0, h1, J], dtype)
    q0, q1 = common.fix_type([q0, q1], np.int8)

    E = np.ndarray((1), dtype)
    cext.bipartite_graph_calculate_E_from_spin(E, h0, h1, J, c, q0, q1, dtype)
    return E[0]

def bipartite_graph_batch_calculate_E_from_spin(h0, h1, J, c, q0, q1, cext, dtype) :
    checkers.bipartite_graph.hJc(h0, h1, J, c)
    checkers.bipartite_graph.bits(J, q0, q1)
    h0, h1, W = common.fix_type([h0, h1, J], dtype)
    q0, q1 = common.fix_type([q0, q1], np.int8)
    
    nBatch0 = 1 if len(q0.shape) == 1 else q0.shape[0]
    nBatch1 = 1 if len(q1.shape) == 1 else q1.shape[0]
    if nBatch0 == 1 :
        q0, q1 = q0.reshape(1, -1), q1.reshape(1, -1)
    E = np.empty((nBatch0), dtype)
    cext.bipartite_graph_batch_calculate_E_from_spin(E, h0, h1, J, c, q0, q1, dtype)
    return E
