// created by Xiaojun Guo, 03/01/25. Currently, it is used in mrcProfiler to get
// reuse distance

#ifndef INCLUDE_SPLAYTREE_HPP
#define INCLUDE_SPLAYTREE_HPP

template <typename K, typename V>
class SplayTree {
  struct node {
    node() : parent(nullptr) {}
    node(K key, V val)
        : parent(nullptr), first(key), second(val), sum(val), size(1) {}
    std::unique_ptr<node> left, right;
    node *parent;
    K first;
    V second;
    V sum;
    uint32_t size;  // size of childs
    void maintain() {
      size = 1;
      sum = second;
      if (left) {
        size += left->size;
        sum += left->sum;
      }
      if (right) {
        size += right->size;
        sum += right->sum;
      }
    }
  };
  typedef typename std::unique_ptr<node> node_ptr;

 public:
  class iterator {
   public:
    iterator() : ptr_(nullptr) {}
    iterator(node *ptr) : ptr_(ptr) {}
    iterator(const iterator &it) { ptr_ = it.ptr_; }
    iterator &operator=(iterator it) {
      if (this == &it) return *this;
      ptr_ = it.ptr_;
      return *this;
    }

    iterator &operator--() {
      node *prev = ptr_;
      if (ptr_ != nullptr) {
        if (ptr_->left != nullptr) {
          ptr_ = ptr_->left.get();
          while (ptr_->right != nullptr) {
            ptr_ = ptr_->right.get();
          }
        } else {
          ptr_ = ptr_->parent;
          while (ptr_ != nullptr && ptr_->left.get() == prev) {
            prev = ptr_;
            ptr_ = ptr_->parent;
          }
        }
      }
      return *this;
    }
    iterator &operator++() {
      node *prev = ptr_;
      if (ptr_ != nullptr) {
        if (ptr_->right != nullptr) {
          ptr_ = ptr_->right.get();
          while (ptr_->left) ptr_ = (ptr_->left).get();
        } else {
          ptr_ = ptr_->parent;
          while (ptr_ != nullptr && ptr_->right.get() == prev) {
            prev = ptr_;
            ptr_ = ptr_->parent;
          }
        }
      }
      return *this;
    }
    iterator operator++(int junk) {
      iterator prev = *this;
      ++(*this);
      return prev;
    }
    iterator operator--(int junk) {
      iterator prev = *this;
      --(*this);
      return prev;
    }
    std::pair<K, V> operator*() {
      return std::make_pair(ptr_->first, ptr_->second);
    }
    node *operator->() { return ptr_; }
    bool operator==(const iterator &rhs) { return ptr_ == rhs.ptr_; }
    bool operator!=(const iterator &rhs) { return ptr_ != rhs.ptr_; }

   private:
    node *ptr_;
  };

 public:
  SplayTree() : sum(0), node_count_{0} {}
  ~SplayTree() {}
  void insert(const K &key, const V &val);
  iterator find(const K &key) {
    return find_(key) ? iterator(find_(key)) : end();
  }
  V getK(int k) { return getK_(root_.get(), k); }
  int getRank(K key) { return getRank_(root_.get(), key); }

  V getSum(K key) { return getSum_(root_.get(), key); }
  V getDistance(K key) { return getDistance_(root_.get(), key); }

  void erase(const K &key) { remove(find_(key)); }
  void erase(iterator pos) { remove(pos.operator->()); }
  void get_dot(std::ostream &o) { getDOT(o); }
  void clear() { purgeTree(root_); }
  bool empty() const { return node_count_ == 0 ? true : false; }
  uint32_t size() const { return node_count_; }
  iterator begin() {
    return node_count_ == 0 ? end() : iterator(subtreeMin(root_.get()));
  }
  iterator end() { return iterator(); }
  void swap(SplayTree &other) {
    node_ptr temp = std::move(root_);
    root_ = std::move(other.root_);
    other.root_ = std::move(temp);
  }
  V sum;

 private:
  uint32_t node_count_;

  node_ptr root_;
  void leftRotate(node *x);
  void rightRotate(node *x);
  void splay(node *x);
  V getK_(node *x, int k);
  int getRank_(node *x, K k);
  V getSum_(node *x, K key);
  V getDistance_(node *x, K key);
  node *subtreeMax(node *p) const;  // find max in subtree
  node *subtreeMin(node *p) const;  // find min in subtree
  node *find_(const K &key);
  node *find_(const K &key) const { return find_(key); }
  void purgeTree(node_ptr &n);
  void getDOT(std::ostream &o);
  void remove(node *n);
};

template <typename K, typename V>
void SplayTree<K, V>::leftRotate(node *rt) {
  node *pivot = rt->right.get();
  node_ptr pv = std::move(rt->right);
  node *grand_pa = rt->parent;
  if (pv->left) {
    rt->right = std::move(pv->left);
    rt->right->parent = rt;
  }

  if (!grand_pa) {
    pv->left = std::move(root_);
    root_ = std::move(pv);
    pivot->parent = nullptr;
    rt->parent = pivot;
  } else {
    if (grand_pa->right.get() == rt) {
      pv->left = std::move(grand_pa->right);
      grand_pa->right = std::move(pv);
    } else {
      pv->left = std::move(grand_pa->left);
      grand_pa->left = std::move(pv);
    }
    pivot->parent = grand_pa;
    rt->parent = pivot;
  }
  rt->maintain();
  pivot->maintain();
  if (grand_pa) grand_pa->maintain();
}

template <typename K, typename V>
void SplayTree<K, V>::rightRotate(node *rt) {
  node *pivot = rt->left.get();
  node_ptr pv = std::move(rt->left);
  node *grand_pa = rt->parent;
  if (pv->right) {  // right node of pv to left node of rt
    rt->left = std::move(pv->right);
    rt->left->parent = rt;
  }
  if (!grand_pa) {  // if rt is really root of the tree
    pv->right = std::move(root_);
    root_ = std::move(pv);
    pivot->parent = nullptr;
    rt->parent = pivot;
  } else {
    if (grand_pa->right.get() ==
        rt) {  // rt is in right branch from his grandparent
      pv->right = std::move(grand_pa->right);
      grand_pa->right = std::move(pv);
      pivot->parent = grand_pa;
      rt->parent = pivot;
    } else {  // rt is in left branch from his grandparent
      pv->right = std::move(grand_pa->left);
      grand_pa->left = std::move(pv);
      pivot->parent = grand_pa;
      rt->parent = pivot;
    }
  }
  rt->maintain();
  pivot->maintain();
  if (grand_pa) grand_pa->maintain();
}

template <typename K, typename V>
void SplayTree<K, V>::splay(node *x) {
  if (x == nullptr) return;
  while (x->parent) {
    if (!x->parent->parent)  // zig step
    {
      if (x == x->parent->left.get())
        rightRotate(x->parent);
      else
        leftRotate(x->parent);
    } else if (x == x->parent->left.get() &&
               x->parent == x->parent->parent->left.get())  // zig zig step left
    {
      rightRotate(x->parent->parent);
      rightRotate(x->parent);
    } else if (x == x->parent->right.get() &&
               x->parent ==
                   x->parent->parent->right.get())  // zig zig step right
    {
      leftRotate(x->parent->parent);
      leftRotate(x->parent);
    } else if (x == x->parent->left.get() &&
               x->parent ==
                   x->parent->parent->right.get())  // zig zag rotation left
    {
      rightRotate(x->parent);
      leftRotate(x->parent);
    } else  // zig zag rotation righ
    {
      leftRotate(x->parent);
      rightRotate(x->parent);
    }
  }
}

template <typename K, typename V>
V SplayTree<K, V>::getK_(node *rt, int k) {
  if (rt->left) {
    if (rt->left->size >= k)
      return getK_(rt->left.get(), k);
    else
      k -= rt->left->size;
  }
  if (k == 1) return rt->second;
  return getK_(rt->right.get(), k - 1);
}

template <typename K, typename V>
int SplayTree<K, V>::getRank_(node *rt, K k) {
  int rs = 0;
  if (k < rt->first) return getRank_(rt->left.get(), k);
  if (rt->left) rs += rt->left->size;
  if (k == rt->first) {
    return rs + 1;
  } else {
    return rs + 1 + getRank_(rt->right.get(), k);
  }
}

template <typename K, typename V>
V SplayTree<K, V>::getSum_(node *rt, K key) {
  V rs = V(0);
  if (key < rt->first) return getSum_(rt->left.get(), key);
  if (rt->left) rs = rt->left->sum;
  if (key == rt->first)
    return rs;
  else
    return rs + rt->second + getSum_(rt->right.get(), key);
}
template <typename K, typename V>
V SplayTree<K, V>::getDistance_(node *x, K key) {
  if (x == nullptr) return 0;
  if (key > x->first) {
    return getDistance_(x->right.get(), key);
  } else if (key == x->first) {
    if (x->left) {
      return x->sum - x->left->sum;
    } else {
      return x->sum;
    }
  } else {
    if (x->left) {
      return getDistance_(x->left.get(), key) + x->sum - x->left->sum;
    } else {
      return getDistance_(x->left.get(), key) + x->sum;
    }
  }
}

template <typename K, typename V>
typename SplayTree<K, V>::node *SplayTree<K, V>::subtreeMax(
    node *p) const  // find max in subtree
{
  node *t = p;
  while (t->right) t = (t->right).get();
  return t;
}

template <typename K, typename V>
typename SplayTree<K, V>::node *SplayTree<K, V>::subtreeMin(
    node *p) const  // find min in subtree
{
  node *t = p;
  while (t->left) t = t->left.get();
  return t;
}

template <typename K, typename V>
void SplayTree<K, V>::purgeTree(node_ptr &n) {
  n.release();
}

template <typename K, typename V>
void SplayTree<K, V>::insert(const K &key, const V &val) {
  node *p = root_.get();
  node *prev = nullptr;
  while (p) {
    prev = p;
    if (key == p->first) {
      p->second = val;
      return;
    } else if (key < p->first)
      p = p->left.get();
    else if (key > p->first)
      p = p->right.get();
  }
  node_ptr n(new node(key, val));

  ++node_count_;
  sum += val;

  if (prev == nullptr) {
    root_ = std::move(n);
  } else if (key < prev->first) {
    prev->left = std::move(n);
    prev->left->parent = prev;
    splay(prev->left.get());
  } else {
    prev->right = std::move(n);
    prev->right->parent = prev;
    splay(prev->right.get());
  }
}

template <typename K, typename V>
typename SplayTree<K, V>::node *SplayTree<K, V>::find_(const K &key) {
  node *p = root_.get();
  while (p) {
    if (key < p->first)
      p = p->left.get();
    else if (key > p->first)
      p = p->right.get();
    else {
      splay(p);
      return root_.get();
    }
  }
  return nullptr;
}

template <typename K, typename V>
void SplayTree<K, V>::remove(node *n) {
  if (n == nullptr) return;
  splay(n);
  --node_count_;
  sum = sum - n->second;
  if (node_count_ == 0) {
    root_.release();
    return;
  }
  if (!n->left) {
    root_ = std::move(n->right);
    root_->parent = nullptr;
    return;
  } else if (!n->right) {
    root_ = std::move(n->left);
    root_->parent = nullptr;
  } else {
    node *nxt = subtreeMin(n->right.get());
    splay(nxt);
    root_->left = std::move(root_->left->left);
    root_->left->parent = root_.get();
  }
  root_->maintain();
}

template <typename K, typename V>
void SplayTree<K, V>::getDOT(std::ostream &o) {
  o << "digraph G{ graph[ordering = out];" << std::endl;
  if (node_count_ == 1)
    o << root_->first << ";" << std::endl;
  else {
    for (auto it = begin(); it != end(); ++it) {
      // 1[label="1 (size=1)"];
      char out[30];
      sprintf(out, "%d[label=\"%d (size=%d)\"];", it->first, it->first,
              (int)it->size);
      o << out << std::endl;
      if (it->parent) {
        std::string type =
            (iterator(it->parent->right.get()) == it ? "right" : "left");
        o << it->parent->first << " -> " << it->first << "[label=\"" << type
          << "\"];" << std::endl;
      }
    }
  }
  o << "}" << std::endl;
}

#endif