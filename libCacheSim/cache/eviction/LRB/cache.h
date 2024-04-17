#ifndef LRB_CACHE_H
#define LRB_CACHE_H

#include <map>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "request.h"
#include "common.h"

namespace webcachesim {

    class Cache;

    class CacheFactory {
    public:
        CacheFactory() {}

        virtual std::unique_ptr<Cache> create_unique() = 0;
    };

    class Cache {
    public:
        // create and destroy a cache
        Cache()
                : _cacheSize(0),
                  _currentSize(0) {
        }

        virtual ~Cache() = default;

        // main cache management functions (to be defined by a policy)
        virtual bool lookup(const SimpleRequest &req) = 0;
        // check whether an object in a cache. Not update metadata
        virtual bool exist(const int64_t &key) {
            throw runtime_error("Error: exist() function not implemented");
        }

        virtual void admit(const SimpleRequest &req) = 0;

        // configure cache parameters
        virtual void setSize(const uint64_t &cs) {
            _cacheSize = cs;
            //delay eviction because not all algorithms implement such interface
        }

        virtual void init_with_params(const map<string, string> &params) {}

        virtual bool has(const uint64_t &id) {
            return false;
        }

        virtual void update_stat_periodic() {
        }

        virtual size_t memory_overhead() {
            return sizeof(Cache);
        }

        uint64_t getCurrentSize() const {
            return (_currentSize);
        }

        uint64_t getSize() const {
            return (_cacheSize);
        }

        // helper functions (factory pattern)
        static void registerType(std::string name, CacheFactory *factory) {
            get_factory_instance()[name] = factory;
        }

        static std::unique_ptr<Cache> create_unique(std::string name) {
            std::unique_ptr<Cache> Cache_instance;
            if (get_factory_instance().count(name) != 1) {
                std::cerr << "unknown cacheType" << std::endl;
                return nullptr;
            }
            Cache_instance = move(get_factory_instance()[name]->create_unique());
            return Cache_instance;
        }

        // basic cache properties
        uint64_t _cacheSize; // size of cache in bytes
        uint64_t _currentSize; // total size of objects in cache in bytes

        // helper functions (factory pattern)
        static std::map<std::string, CacheFactory *> &get_factory_instance() {
            static std::map<std::string, CacheFactory *> map_instance;
            return map_instance;
        }
    };

    template<class T>
    class Factory : public CacheFactory {
    public:
        Factory(std::string name) { Cache::registerType(name, this); }

        std::unique_ptr<Cache> create_unique() {
            std::unique_ptr<Cache> newT(new T);
            return newT;
        }
    };
}

#endif /* LRB_CACHE_H */