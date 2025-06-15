// created by Xiaojun Guo, 03/03/25. Currently used for online Belady implementation

#ifndef SEGMENT_TREE_HPP
#define SEGMENT_TREE_HPP
#include <stdint.h>
#include <unordered_map>
#include <algorithm>
#include <vector>

using namespace std;

class SegmentTree {
private:
    vector<int64_t> tree, lazy;
    int size;

    int get_left(int node, int granularity) {
        return node - granularity;
    }

    int get_right(int node, int granularity) {
        return node - 1;
    }

    int get_root(){
        return 2*size - 1;
    }

    void updateRange(int node, int start, int end, int l, int r, int64_t val) {
        int64_t granularity = end - start + 1;
        if(lazy[node] != 0) {
            tree[node] += lazy[node];
            if(start != end) {
                lazy[get_left(node, granularity)] += lazy[node];
                lazy[get_right(node, granularity)] += lazy[node];
            }
            lazy[node] = 0;
        }
        if(start > end || start > r || end < l)
            return;
        if(start >= l && end <= r) {
            tree[node] += val;
            if(start != end) {
                lazy[get_left(node, granularity)] += val;
                lazy[get_right(node, granularity)] += val;
            }
            return;
        }
        int mid = (start + end) / 2;
        updateRange(get_left(node, granularity), start, mid, l, r, val);
        updateRange(get_right(node, granularity), mid + 1, end, l, r, val);
        tree[node] = max(tree[get_left(node, granularity)], tree[get_right(node, granularity)]);
    }

    int64_t queryRange(int node, int start, int end, int l, int r) {
        if(start > end || start > r || end < l)
            return 0; 
        int64_t granularity = end - start + 1;
        if(lazy[node] != 0) {
            tree[node] += lazy[node];
            if(start != end) {
                lazy[get_left(node, granularity)] += lazy[node];
                lazy[get_right(node, granularity)] += lazy[node];
            }
            lazy[node] = 0;
        }
        if(start >= l && end <= r)
            return tree[node];
        int mid = (start + end) / 2;
        int64_t p1 = queryRange(get_left(node, granularity), start, mid, l, r);
        int64_t p2 = queryRange(get_right(node, granularity), mid + 1, end, l, r);
        return max(p1, p2);
    }

public:
    SegmentTree() {
        int initial_size = 1;
        tree.resize(initial_size * 2, 0);
        lazy.resize(initial_size * 2, 0);
        size = initial_size;
    }

    void double_size() {
        int old_max = tree[get_root()];
        size *= 2;
        tree.resize(size * 2, 0);
        lazy.resize(size * 2, 0);
        tree[get_root()] = old_max;
    }

    void resize_to(int new_size) {
        while(size < new_size)
            double_size();
    }

    void update(int l, int r, int64_t val) {
        if(r >= size){
            resize_to(r + 1);
        }

        updateRange(get_root(), 0, size-1, l, r, val);
    }

    int64_t query(int l, int r) {
        return queryRange(get_root(), 0, size-1, l, r);
    }
};

#endif