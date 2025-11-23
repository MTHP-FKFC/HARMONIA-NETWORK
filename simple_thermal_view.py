#!/usr/bin/env python3
"""
Simple Thermal Analysis Viewer - Creates viewable graphs
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Load data
df = pd.read_csv('thermal_debug.csv')
print(f"📊 Loaded {len(df)} data points")

# Create simple, viewable plots
plt.style.use('default')  # Use default style for compatibility

# 1. Temperature over time
plt.figure(figsize=(12, 8))
plt.subplot(2, 2, 1)
plt.plot(df['Time'], df['Temperature'], 'r-', linewidth=2)
plt.title('🔥 Temperature Over Time')
plt.xlabel('Time (seconds)')
plt.ylabel('Temperature (°C)')
plt.grid(True, alpha=0.3)

# 2. Drive level
plt.subplot(2, 2, 2)
plt.plot(df['Time'], df['Drive'], 'b-', linewidth=2)
plt.title('⚡ Drive Level')
plt.xlabel('Time (seconds)')
plt.ylabel('Drive')
plt.grid(True, alpha=0.3)

# 3. Input vs Output
plt.subplot(2, 2, 3)
plt.plot(df['Time'], df['Input'], 'g-', alpha=0.7, label='Input')
plt.plot(df['Time'], df['Output'], 'r-', alpha=0.7, label='Output')
plt.title('📊 Input vs Output')
plt.xlabel('Time (seconds)')
plt.ylabel('Amplitude')
plt.legend()
plt.grid(True, alpha=0.3)

# 4. Temperature vs Drive scatter
plt.subplot(2, 2, 4)
plt.scatter(df['Drive'], df['Temperature'], alpha=0.5, s=1)
plt.title('🌡️ Temperature vs Drive')
plt.xlabel('Drive Level')
plt.ylabel('Temperature (°C)')
plt.grid(True, alpha=0.3)

plt.suptitle('HARMONIA NETWORK - Thermal Analysis', fontsize=16, fontweight='bold')
plt.tight_layout()

# Save with standard settings
plt.savefig('thermal_simple.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✅ Saved thermal_simple.png")

# Open the file
import subprocess
subprocess.run(['open', 'thermal_simple.png'])

# Show summary
print(f"\n📈 Summary:")
print(f"   Temperature Range: {df['Temperature'].min():.1f}°C - {df['Temperature'].max():.1f}°C")
print(f"   Drive Range: {df['Drive'].min():.2f} - {df['Drive'].max():.2f}")
print(f"   Duration: {df['Time'].max():.1f} seconds")