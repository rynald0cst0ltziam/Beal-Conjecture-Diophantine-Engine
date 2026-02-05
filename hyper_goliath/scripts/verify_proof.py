#!/usr/bin/env python3
"""
Hyper-Goliath Run Verifier
Verifies the mathematical integrity of a search run by recomputing its FNV-1a hash.
"""
import json
import sys
import os

def fnv1a_64(data_list):
    hash_val = 14695981039346656037
    FNV_PRIME = 1099511628211
    MASK = 0xFFFFFFFFFFFFFFFF
    
    for val in data_list:
        hash_val ^= val
        hash_val = (hash_val * FNV_PRIME) & MASK
    return hash_val

def verify_log(log_path):
    print(f"🧐 Verifying log: {log_path}")
    
    start_event = None
    complete_event = None
    
    try:
        with open(log_path, 'r') as f:
            for line in f:
                event = json.loads(line)
                if event.get("event") == "START":
                    start_event = event
                elif event.get("event") == "COMPLETE":
                    complete_event = event
    except Exception as e:
        print(f"❌ Error reading log: {e}")
        return False
        
    if not start_event or not complete_event:
        print("❌ Error: Log is missing START or COMPLETE event.")
        return False
        
    # Extract parameters for hash recomputation
    # Parameters must match the order in logging.c:log_complete
    params = [
        start_event["signature"][0],    # x
        start_event["signature"][1],    # y
        start_event["signature"][2],    # z
        start_event["Astart"],          # A_start
        start_event["Amax"],            # A_max
        start_event["Bstart"],          # B_start
        start_event["Bmax"],            # B_max
        start_event["Cmax"],            # C_max
        complete_event["results"]["total_pairs"],
        complete_event["results"]["gcd_filtered"],
        complete_event["results"]["mod_filtered"],
        complete_event["results"]["exact_checks"],
        complete_event["results"]["power_hits"],
        complete_event["results"]["primitive_counterexamples"]
    ]
    
    computed_hash = fnv1a_64(params)
    logged_hash_hex = complete_event["verification"]["integrity_hash"]
    logged_hash = int(logged_hash_hex, 16)
    
    print(f"  Signature: {start_event['signature']}")
    print(f"  Range:     A[{start_event['Astart']}-{start_event['Amax']}] B[{start_event['Bstart']}-{start_event['Bmax']}]")
    print(f"  Pairs:     {complete_event['results']['total_pairs']:,}")
    print(f"  Status:    {complete_event['verification']['status']}")
    
    if computed_hash == logged_hash:
        print(f"\n✅ VERIFIED: Hash {logged_hash_hex} is mathematically correct.")
        print("This run's parameters and results are internally consistent.")
        return True
    else:
        print(f"\n🚨 FAILED: Hash mismatch!")
        print(f"  Computed: {computed_hash:016x}")
        print(f"  Logged:   {logged_hash_hex}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 verify_proof.py <path_to_jsonl>")
        sys.exit(1)
    verify_log(sys.argv[1])
