use crate::bindings::*;
use std::ffi::CString;
use std::ptr;


pub struct Cache {
    cache_ptr: *mut cache_t,
}

impl Cache {
    /// Initializes a new LRU cache.
    pub fn LRU_init(params: common_cache_params_t, specific_params: &str) -> Option<Self> {
        let c_params = CString::new(specific_params).expect("CString conversion failed");
        let cache_ptr = unsafe { LRU_init(params, c_params.as_ptr()) };

        if cache_ptr.is_null() {
            None
        } else {
            Some(Self { cache_ptr })
        }
    }

    /// Frees the cache.
    pub fn free(&mut self) {
        if !self.cache_ptr.is_null() {
            unsafe { 
                let cache_ref = *self.cache_ptr;
                // Ensure the function pointer is valid, then call it
                if !cache_ref.cache_free.is_none() {
                    (cache_ref.cache_free.unwrap())(self.cache_ptr);
                    self.cache_ptr = std::ptr::null_mut(); // Invalidate the pointer after freeing
                } else {
                    eprintln!("cache_free function pointer is null!");
                }
            };
        }
    }

    /// Checks if a request hits the cache.
    pub fn get(&self, req: &Request) -> bool {
        if self.cache_ptr.is_null() {
            return false;
        }
        unsafe { 
            let cache_ref = *self.cache_ptr;
            // Ensure the function pointer is valid, then call it
            if !cache_ref.get.is_none() {
                (cache_ref.get.unwrap())(self.cache_ptr, req.as_raw())
            } else {
                eprintln!("get function pointer is null!");
                false
            }
        }
    }

    /// Finds an object in the cache, optionally updating it.
    pub fn find(&self, req: &Request, update_cache: bool) -> Option<*mut cache_obj_t> {
        if self.cache_ptr.is_null() {
            return None;
        }
        let obj_ptr = unsafe { 
            let cache_ref = *self.cache_ptr;
            // Ensure the function pointer is valid, then call it
            if !cache_ref.find.is_none() {
                (cache_ref.find.unwrap())(self.cache_ptr, req.as_raw(), update_cache)
            } else {
                eprintln!("find function pointer is null!");
                ptr::null_mut()
            }
        };
        if obj_ptr.is_null() {
            None
        } else {
            Some(obj_ptr)
        }
    }

    /// Inserts a new object into the cache.
    pub fn insert(&mut self, req: &Request) -> Option<*mut cache_obj_t> {
        if self.cache_ptr.is_null() {
            return None;
        }
        let obj_ptr = unsafe { 
            let cache_ref = *self.cache_ptr;
            // Ensure the function pointer is valid, then call it
            if !cache_ref.insert.is_none() {
                (cache_ref.insert.unwrap())(self.cache_ptr, req.as_raw())
            } else {
                eprintln!("insert function pointer is null!");
                ptr::null_mut()
            }
        };
        if obj_ptr.is_null() {
            None
        } else {
            Some(obj_ptr)
        }
    }

    /// Determines which object to evict.
    pub fn to_evict(&self, req: &Request) -> Option<*mut cache_obj_t> {
        if self.cache_ptr.is_null() {
            return None;
        }
        let obj_ptr = unsafe { 
            let cache_ref = *self.cache_ptr;
            // Ensure the function pointer is valid, then call it
            if !cache_ref.to_evict.is_none() {
                (cache_ref.to_evict.unwrap())(self.cache_ptr, req.as_raw())
            } else {
                eprintln!("to_evict function pointer is null!");
                ptr::null_mut()
            }
        };
        if obj_ptr.is_null() {
            None
        } else {
            Some(obj_ptr)
        }
    }

    /// Evicts an object from the cache.
    pub fn evict(&mut self, req: &Request) {
        if !self.cache_ptr.is_null() {
            unsafe { 
                let cache_ref = *self.cache_ptr;
                // Ensure the function pointer is valid, then call it
                if !cache_ref.evict.is_none() {
                    (cache_ref.evict.unwrap())(self.cache_ptr, req.as_raw())
                } else {
                    eprintln!("evict function pointer is null!");
                }          
            };
        }
    }

    /// Removes an object from the cache by its ID.
    pub fn remove(&mut self, obj_id: obj_id_t) -> bool {
        if self.cache_ptr.is_null() {
            return false;
        }
        unsafe { 
            let cache_ref = *self.cache_ptr;
            // Ensure the function pointer is valid, then call it
            if !cache_ref.remove.is_none() {
                (cache_ref.remove.unwrap())(self.cache_ptr, obj_id)
            } else {
                eprintln!("remove function pointer is null!");
                false
            }
        }
    }

    pub fn print_cache(&self) {
        if !self.cache_ptr.is_null() {
            unsafe { 
                let cache_ref = *self.cache_ptr;
                // Ensure the function pointer is valid, then call it
                if !cache_ref.print_cache.is_none() {
                    (cache_ref.print_cache.unwrap())(self.cache_ptr);
                } else {
                    eprintln!("print_cache function pointer is null!");
                }
            };
        }
    }
}

// Ensure the cache gets freed properly when dropped.
impl Drop for Cache {
    fn drop(&mut self) {
        self.free();
    }
}


pub struct Reader {
    reader_ptr: *mut reader_t,
}


impl Reader {
    fn from_raw(ptr: *mut reader_t) -> Option<Self> {
        if ptr.is_null() {
            None
        } else {
            Some(Self { reader_ptr: ptr })
        }
    }

    pub fn set_default_reader_init_params(params: &mut reader_init_param_t) {
        unsafe { set_default_reader_init_params(params) };
    }

    pub fn default_reader_init_params() -> reader_init_param_t {
        unsafe { default_reader_init_params() }
    }

    pub fn setup_reader(trace_path: &str, trace_type: trace_type_e, init_params: &reader_init_param_t) -> Option<Self> {
        let path = CString::new(trace_path).unwrap();
        let ptr = unsafe { setup_reader(path.as_ptr(), trace_type, init_params) };
        Self::from_raw(ptr)
    }

    pub fn open_trace(trace_path: &str, trace_type: trace_type_e, init_params: &reader_init_param_t) -> Option<Self> {
        let path = CString::new(trace_path).unwrap();
        let ptr = unsafe { open_trace(path.as_ptr(), trace_type, init_params) };
        Self::from_raw(ptr)
    }

    pub fn get_num_of_req(&self) -> u64 {
        if self.reader_ptr.is_null() {
            return 0;
        }
        unsafe { get_num_of_req(self.reader_ptr) }
    }

    pub fn get_trace_type(&self) -> trace_type_e {
        unsafe { get_trace_type(self.reader_ptr) }
    }

    pub fn obj_id_is_num(&self) -> bool {
        unsafe { obj_id_is_num(self.reader_ptr) }
    }

    pub fn read_one_req(&mut self, req: &Request) -> i32 {
        unsafe { read_one_req(self.reader_ptr, req.as_raw()) }
    }

    pub fn read_trace(&mut self, req: &Request) -> i32 {
        unsafe { read_trace(self.reader_ptr, req.as_raw())}
    }

    pub fn reset_reader(&mut self) {
        unsafe { reset_reader(self.reader_ptr) };
    }

    pub fn close_reader(&mut self) -> bool {
        let result = unsafe { close_reader(self.reader_ptr) };
        self.reader_ptr = ptr::null_mut();
        result == 0
    }

    pub fn close_trace(&mut self) -> i32 {
        unsafe { close_trace(self.reader_ptr) }
    }

    pub fn clone_reader(&self) -> Option<Self> {
        let ptr = unsafe { clone_reader(self.reader_ptr) };
        Self::from_raw(ptr)
    }

    pub fn read_first_req(&mut self, req: &Request) {
        unsafe { read_first_req(self.reader_ptr, req.as_raw()) };
    }

    pub fn read_last_req(&mut self, req: &Request) {
        unsafe { read_last_req(self.reader_ptr, req.as_raw()) };
    }

    pub fn skip_n_req(&mut self, n: i32) -> i32 {
        unsafe { skip_n_req(self.reader_ptr, n) }
    }

    pub fn read_one_req_above(&mut self, req: &Request) -> i32 {
        unsafe { read_one_req_above(self.reader_ptr, req.as_raw()) }
    }

    pub fn go_back_one_req(&mut self) -> i32 {
        unsafe { go_back_one_req(self.reader_ptr) }
    }

    pub fn reader_set_read_pos(&mut self, pos: f64) {
        unsafe { reader_set_read_pos(self.reader_ptr, pos) };
    }

    pub fn print_reader(&self) {
        unsafe { print_reader(self.reader_ptr) };
    }
}

impl Drop for Reader {
    fn drop(&mut self) {
        self.close_reader();
    }
}

pub struct Request(*mut request_t);

impl Request {
    /// Creates a new request
    pub fn new_request() -> Option<Self> {
        let req = unsafe { new_request() };
        if req.is_null() {
            None
        } else {
            Some(Self(req))
        }
    }

    pub fn as_raw(&self) -> *mut request_t {
        self.0
    }

    /// Copies data from another request
    pub fn copy_request(&mut self, source: &Request) {
        unsafe { copy_request(self.0, source.0) };
    }

    /// Clones the current request
    pub fn clone_request(&self) -> Option<Self> {
        let req_clone = unsafe { clone_request(self.0) };
        if req_clone.is_null() {
            None
        } else {
            Some(Self(req_clone))
        }
    }

    /// Prints the request
    pub fn print_request(&self) {
        unsafe { print_request(self.0) };
    }
}

impl Drop for Request {
    /// Frees memory when Request is dropped
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe { free_request(self.0) };
        }
    }
}

impl Default for common_cache_params_t {
    fn default() -> Self {
        Self {
            cache_size: 0,
            default_ttl: 0,
            hashpower: 0,
            consider_obj_metadata: false,
        }
    }
}

