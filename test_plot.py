#!/usr/bin/env python3
"""
Simple test to verify matplotlib can create viewable images
"""

import matplotlib.pyplot as plt
import numpy as np

# Create a simple test plot
plt.figure(figsize=(10, 6))
x = np.linspace(0, 10, 100)
y = np.sin(x)

plt.plot(x, y, 'b-', linewidth=2)
plt.title('Test Plot - Should Open Normally')
plt.xlabel('Time')
plt.ylabel('Value')
plt.grid(True)

# Save with different settings
plt.savefig('test_plot.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✅ Test plot saved as test_plot.png")

# Try to open it
import subprocess
subprocess.run(['open', 'test_plot.png'])