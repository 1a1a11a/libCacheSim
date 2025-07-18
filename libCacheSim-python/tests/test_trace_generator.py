#!/usr/bin/env python3
"""
Tests for trace generator module.
"""

import libcachesim as lcs


class TestTraceGeneration:
    """Test trace generation functions."""

    # Constants for test readability
    NUM_SAMPLE_REQUESTS = 10  # Number of requests to check in detail

    def test_create_zipf_requests_basic(self):
        """Test basic Zipf request creation."""
        generator = lcs.create_zipf_requests(num_objects=100, num_requests=1000, alpha=1.0, obj_size=4000, seed=42)

        # Test iteration
        requests = list(generator)
        assert len(requests) == 1000

        for req in requests[: self.NUM_SAMPLE_REQUESTS]:  # Check first NUM_SAMPLE_REQUESTS
            assert isinstance(req, lcs.Request)
            assert 0 <= req.obj_id < 100
            assert req.obj_size == 4000
            assert req.clock_time >= 0

    def test_create_uniform_requests_basic(self):
        """Test basic uniform request creation."""
        generator = lcs.create_uniform_requests(num_objects=100, num_requests=1000, obj_size=4000, seed=42)

        # Test iteration
        requests = list(generator)
        assert len(requests) == 1000

        for req in requests[: self.NUM_SAMPLE_REQUESTS]:  # Check first NUM_SAMPLE_REQUESTS
            assert isinstance(req, lcs.Request)
            assert 0 <= req.obj_id < 100
            assert req.obj_size == 4000
            assert req.clock_time >= 0

    def test_zipf_reproducibility(self):
        """Test reproducibility with seed."""
        gen1 = lcs.create_zipf_requests(10, 100, alpha=1.0, seed=42)
        gen2 = lcs.create_zipf_requests(10, 100, alpha=1.0, seed=42)

        requests1 = list(gen1)
        requests2 = list(gen2)

        assert len(requests1) == len(requests2)
        for req1, req2 in zip(requests1, requests2):
            assert req1.obj_id == req2.obj_id

    def test_uniform_reproducibility(self):
        """Test reproducibility with seed."""
        gen1 = lcs.create_uniform_requests(10, 100, seed=42)
        gen2 = lcs.create_uniform_requests(10, 100, seed=42)

        requests1 = list(gen1)
        requests2 = list(gen2)

        assert len(requests1) == len(requests2)
        for req1, req2 in zip(requests1, requests2):
            assert req1.obj_id == req2.obj_id

    def test_different_seeds(self):
        """Test that different seeds produce different results."""
        gen1 = lcs.create_zipf_requests(10, 100, alpha=1.0, seed=42)
        gen2 = lcs.create_zipf_requests(10, 100, alpha=1.0, seed=43)

        requests1 = [req.obj_id for req in gen1]
        requests2 = [req.obj_id for req in gen2]

        assert requests1 != requests2

    def test_zipf_with_cache(self):
        """Test Zipf generator with cache simulation."""
        cache = lcs.LRU(cache_size=50 * 1024)  # 50KB cache
        generator = lcs.create_zipf_requests(
            num_objects=100,
            num_requests=1000,
            alpha=1.0,
            obj_size=1000,  # 1KB objects
            seed=42,
        )

        hit_count = 0
        for req in generator:
            if cache.get(req):
                hit_count += 1

        # Should have some hits and some misses
        assert 0 <= hit_count <= 1000
        assert hit_count > 0  # Should have some hits

    def test_uniform_with_cache(self):
        """Test uniform generator with cache simulation."""
        cache = lcs.LRU(cache_size=50 * 1024)  # 50KB cache
        generator = lcs.create_uniform_requests(
            num_objects=100,
            num_requests=1000,
            obj_size=1000,  # 1KB objects
            seed=42,
        )

        hit_count = 0
        for req in generator:
            if cache.get(req):
                hit_count += 1

        # Should have some hits and some misses
        assert 0 <= hit_count <= 1000
        assert hit_count > 0  # Should have some hits

    def test_custom_parameters(self):
        """Test generators with custom parameters."""
        generator = lcs.create_zipf_requests(
            num_objects=50,
            num_requests=200,
            alpha=1.5,
            obj_size=2048,
            time_span=3600,  # 1 hour
            start_obj_id=1000,
            seed=123,
        )

        requests = list(generator)
        assert len(requests) == 200

        # Check custom parameters
        for req in requests[: self.NUM_SAMPLE_REQUESTS // 2]:  # Check fewer for shorter test
            assert 1000 <= req.obj_id < 1050  # start_obj_id + num_objects
            assert req.obj_size == 2048
            assert req.clock_time <= 3600
