#ifndef _MATRIX_H
#define _MATRIX_H

#include <iostream>
#include <vector>
using namespace std;

template <typename T> class Matrix
{
    unsigned _nrows, _ncols;
    vector<vector<T>> _rows;
public:
    vector<T>& operator [](unsigned i) { return _rows[i]; }
    void dump()
    {
        for(auto row:_rows)
        {
            for(auto cell:row) cout << cell << "\t";
            cout << endl; 
        }
    }
    Matrix(unsigned nrows, unsigned ncols, T initval) : _nrows(nrows), _ncols(ncols), _rows(nrows,vector<T>(ncols, initval))
    {}
};

template <typename T> class SqMatrix : public Matrix<T>
{
public:
    SqMatrix(unsigned dims, T initval) : Matrix<T>(dims,dims,initval) {}
};

#endif
