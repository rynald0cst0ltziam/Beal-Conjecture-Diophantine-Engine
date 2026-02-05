#!/usr/bin/env python3
"""
Deep Cross-Validation Script.
Compares exact survivor sets between Python and C engines.
"""
import json
import subprocess
import os
import sys
import math

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

def get_python_survivors(x, y, z, A_max, B_max):
    """Run Python sieve and return set of survivor pairs."""
    prune_mods = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71]
    res_z_sets = [{pow(r, z, m) for r in range(m)} for m in prune_mods]
    
    survivors = set()
    for A in range(1, A_max + 1):
        for B in range(1, B_max + 1):
            if math.gcd(A, B) > 1:
                continue
            
            killed = False
            for i, m in enumerate(prune_mods):
                s_mod = (pow(A, x, m) + pow(B, y, m)) % m
                if s_mod not in res_z_sets[i]:
                    killed = True
                    break
            
            if not killed:
                survivors.add((A, B))
    return survivors

def get_c_survivors(x, y, z, A_max, B_max, export_binary):
    """Run C export tool and return set of survivor pairs."""
    result = subprocess.run(
        [export_binary, str(x), str(y), str(z), str(A_max), str(B_max)],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"C export failed: {result.stderr}")
        return None
    
    try:
        data = json.loads(result.stdout)
        return {tuple(p) for p in data["survivors"]}
    except Exception as e:
        print(f"Failed to parse C output: {e}")
        # Print raw output for debugging
        print("Raw C output head:", result.stdout[:200])
        return None

def main():
    x, y, z = 3, 4, 5
    A_max, B_max = 100, 100
    
    current_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(current_dir)
    export_binary = os.path.join(project_dir, "build", "export_survivors")
    
    if not os.path.exists(export_binary):
        print("C export tool not found. Build it first.")
        return 1
    
    print(f"Deep Validation: Signature ({x}, {y}, {z}) up to {A_max}")
    print("-" * 50)
    
    print("Computing Python survivors...")
    py_survivors = get_python_survivors(x, y, z, A_max, B_max)
    
    print("Computing C survivors...")
    c_survivors = get_c_survivors(x, y, z, A_max, B_max, export_binary)
    
    if c_survivors is None: return 1
    
    print(f"Python count: {len(py_survivors)}")
    print(f"C count:      {len(c_survivors)}")
    
    if py_survivors == c_survivors:
        print("✅ SUCCESS: Survivor sets match exactly.")
        return 0
    else:
        print("❌ FAILURE: Divergence detected.")
        
        extra_in_c = c_survivors - py_survivors
        missing_in_c = py_survivors - c_survivors
        
        if extra_in_c:
            print(f"Pairs in C but NOT in Python ({len(extra_in_c)}):")
            for p in sorted(list(extra_in_c))[:10]:
                print(f"  {p}")
                # Debug one pair
                A, B = p
                print(f"    Check for {A}, {B}:")
                prune_mods = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71]
                for m in prune_mods:
                    py_sum = (pow(A, x, m) + pow(B, y, m)) % m
                    py_res = {pow(r, z, m) for r in range(m)}
                    if py_sum not in py_res:
                         print(f"      🚨 KILLED by m={m}: (A^x+B^y)%m={py_sum}, R_z={py_res}")
                    else:
                         # For survivors, just show briefly
                         pass
        
        if missing_in_c:
            print(f"Pairs in Python but NOT in C ({len(missing_in_c)}):")
            for p in sorted(list(missing_in_c))[:10]:
                print(f"  {p}")

if __name__ == "__main__":
    main()
