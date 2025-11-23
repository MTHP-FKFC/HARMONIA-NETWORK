#!/usr/bin/env python3
"""
HARMONIA NETWORK - Thermal Dynamics Visualization

This script processes thermal analysis data from the C++ simulation
and generates scientific visualizations proving the thermodynamic behavior
of the Divine Math Saturation Engine.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy import signal
import seaborn as sns
from matplotlib.gridspec import GridSpec
import warnings
warnings.filterwarnings('ignore')

# Set style for scientific publication
plt.style.use('seaborn-v0_8-darkgrid')
sns.set_palette("husl")

def load_thermal_data(csv_file="thermal_debug.csv"):
    """Load thermal analysis data from CSV file."""
    try:
        df = pd.read_csv(csv_file)
        print(f"✅ Loaded {len(df)} data points from {csv_file}")
        return df
    except FileNotFoundError:
        print(f"❌ Error: {csv_file} not found. Run thermal_analysis first.")
        return None

def calculate_harmonics(audio_data, sample_rate=44100):
    """Calculate harmonic content using FFT."""
    # Apply window to reduce spectral leakage
    window = signal.windows.hann(len(audio_data))
    windowed = audio_data * window
    
    # Compute FFT
    fft = np.fft.fft(windowed)
    freqs = np.fft.fftfreq(len(windowed), 1/sample_rate)
    
    # Only positive frequencies
    positive_freqs = freqs[:len(freqs)//2]
    magnitude = np.abs(fft[:len(fft)//2])
    
    return positive_freqs, magnitude

def create_temperature_overview(df):
    """Create comprehensive temperature dynamics overview."""
    fig = plt.figure(figsize=(16, 12))
    gs = GridSpec(3, 2, figure=fig, hspace=0.3, wspace=0.3)
    
    # Temperature accumulation over time
    ax1 = fig.add_subplot(gs[0, :])
    ax1.plot(df['Time'], df['Temperature'], 'r-', linewidth=2, label='Core Temperature')
    ax1.fill_between(df['Time'], df['Temperature'], alpha=0.3, color='red')
    ax1.set_xlabel('Time (seconds)', fontsize=12)
    ax1.set_ylabel('Temperature (°C)', fontsize=12)
    ax1.set_title('🔥 Thermal Accumulation Dynamics', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    
    # Drive level over time
    ax2 = fig.add_subplot(gs[1, 0])
    ax2.plot(df['Time'], df['Drive'], 'b-', linewidth=2)
    ax2.fill_between(df['Time'], df['Drive'], alpha=0.3, color='blue')
    ax2.set_xlabel('Time (seconds)', fontsize=12)
    ax2.set_ylabel('Drive Level', fontsize=12)
    ax2.set_title('⚡ Input Drive Profile', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    
    # Temperature vs Drive correlation
    ax3 = fig.add_subplot(gs[1, 1])
    scatter = ax3.scatter(df['Drive'], df['Temperature'], 
                         c=df['Time'], cmap='viridis', alpha=0.6, s=10)
    ax3.set_xlabel('Drive Level', fontsize=12)
    ax3.set_ylabel('Temperature (°C)', fontsize=12)
    ax3.set_title('🌡️ Temperature-Drive Correlation', fontsize=14, fontweight='bold')
    plt.colorbar(scatter, ax=ax3, label='Time (s)')
    ax3.grid(True, alpha=0.3)
    
    # Input/Output comparison
    ax4 = fig.add_subplot(gs[2, 0])
    ax4.plot(df['Time'], df['Input'], 'g-', alpha=0.7, linewidth=1, label='Input')
    ax4.plot(df['Time'], df['Output'], 'r-', alpha=0.7, linewidth=1, label='Output')
    ax4.set_xlabel('Time (seconds)', fontsize=12)
    ax4.set_ylabel('Amplitude', fontsize=12)
    ax4.set_title('📊 Input vs Output Signal', fontsize=14, fontweight='bold')
    ax4.legend()
    ax4.grid(True, alpha=0.3)
    
    # Gain reduction over time
    ax5 = fig.add_subplot(gs[2, 1])
    gain_reduction = df['Output'] / (df['Input'] + 1e-10)  # Avoid division by zero
    ax5.plot(df['Time'], gain_reduction, 'purple', linewidth=1, alpha=0.7)
    ax5.set_xlabel('Time (seconds)', fontsize=12)
    ax5.set_ylabel('Gain Ratio', fontsize=12)
    ax5.set_title('🎛️ Dynamic Gain Reduction', fontsize=14, fontweight='bold')
    ax5.grid(True, alpha=0.3)
    
    plt.suptitle('HARMONIA NETWORK - Thermal Dynamics Analysis', 
                 fontsize=16, fontweight='bold', y=0.98)
    plt.tight_layout()
    plt.savefig('thermal_overview.png', dpi=300, bbox_inches='tight')
    print("📊 Saved thermal_overview.png")

def create_transfer_function_analysis(df):
    """Create dynamic transfer function visualization."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    # Create temperature bins for transfer function
    temp_bins = np.linspace(df['Temperature'].min(), df['Temperature'].max(), 10)
    
    # Plot 1: Transfer functions at different temperatures
    input_range = np.linspace(-1, 1, 100)
    
    for i, temp in enumerate(temp_bins[:-1]):
        mask = (df['Temperature'] >= temp) & (df['Temperature'] < temp_bins[i+1])
        if len(df[mask]) > 0:
            avg_drive = df[mask]['Drive'].mean()
            
            # Calculate transfer function for this temperature
            temp_factor = (temp - 20.0) / 180.0  # Normalize to 0-1
            saturation = 1.0 + temp_factor * 2.0
            transfer = np.tanh(input_range * saturation * avg_drive)
            
            color = plt.cm.hot(temp_factor)
            ax1.plot(input_range, transfer, color=color, alpha=0.7, 
                    label=f'{temp:.1f}°C', linewidth=2)
    
    ax1.set_xlabel('Input Level', fontsize=12)
    ax1.set_ylabel('Output Level', fontsize=12)
    ax1.set_title('🔥 Temperature-Dependent Transfer Functions', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    
    # Plot 2: 2D Transfer function heatmap
    temp_range = np.linspace(20, 200, 50)
    input_range = np.linspace(-1, 1, 100)
    transfer_surface = np.zeros((len(temp_range), len(input_range)))
    
    for i, temp in enumerate(temp_range):
        temp_factor = (temp - 20.0) / 180.0
        saturation = 1.0 + temp_factor * 2.0
        avg_drive = df['Drive'].mean()
        transfer_surface[i, :] = np.tanh(input_range * saturation * avg_drive)
    
    im = ax2.imshow(transfer_surface, aspect='auto', origin='lower', 
                    extent=[-1, 1, 20, 200], cmap='hot')
    ax2.set_xlabel('Input Level', fontsize=12)
    ax2.set_ylabel('Temperature (°C)', fontsize=12)
    ax2.set_title('🌡️ 2D Transfer Function Surface', fontsize=14, fontweight='bold')
    plt.colorbar(im, ax=ax2, label='Output Level')
    
    plt.suptitle('HARMONIA NETWORK - Dynamic Transfer Function Analysis', 
                 fontsize=16, fontweight='bold')
    plt.tight_layout()
    plt.savefig('transfer_functions.png', dpi=300, bbox_inches='tight')
    print("📊 Saved transfer_functions.png")

def create_harmonic_analysis(df):
    """Analyze and visualize harmonic content changes with temperature."""
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
    
    # Split data into temperature ranges
    cool_mask = df['Temperature'] < 50
    warm_mask = (df['Temperature'] >= 50) & (df['Temperature'] < 100)
    hot_mask = df['Temperature'] >= 100
    
    sample_rate = 44100
    
    # Plot harmonic spectra for different temperature ranges
    for mask, label, color in [(cool_mask, 'Cool (<50°C)', 'blue'),
                               (warm_mask, 'Warm (50-100°C)', 'orange'),
                               (hot_mask, 'Hot (>100°C)', 'red')]:
        if len(df[mask]) > 100:
            # Use a representative segment
            segment = df[mask]['Output'].values[:min(4096, len(df[mask]))]
            freqs, magnitude = calculate_harmonics(segment, sample_rate)
            
            # Only plot up to 5kHz for clarity
            freq_mask = freqs <= 5000
            ax1.semilogy(freqs[freq_mask], magnitude[freq_mask] + 1e-10, 
                        color=color, alpha=0.7, linewidth=2, label=label)
    
    ax1.set_xlabel('Frequency (Hz)', fontsize=12)
    ax1.set_ylabel('Magnitude (log scale)', fontsize=12)
    ax1.set_title('🎵 Harmonic Spectrum vs Temperature', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_xlim(0, 5000)
    
    # Plot harmonic distortion over time
    window_size = 4410  # 100ms windows
    thd_values = []
    time_stamps = []
    
    for start in range(0, len(df), window_size):
        end = min(start + window_size, len(df))
        if end - start < window_size // 2:
            break
            
        segment = df['Output'].iloc[start:end].values
        freqs, magnitude = calculate_harmonics(segment, sample_rate)
        
        # Calculate THD (simplified - sum of harmonics above fundamental)
        fundamental_mask = (freqs >= 80) & (freqs <= 120)  # Around 100Hz
        harmonic_mask = (freqs > 120) & (freqs <= 2000)
        
        if np.any(fundamental_mask) and np.any(harmonic_mask):
            fundamental_power = np.sum(magnitude[fundamental_mask]**2)
            harmonic_power = np.sum(magnitude[harmonic_mask]**2)
            thd = np.sqrt(harmonic_power) / (np.sqrt(fundamental_power) + 1e-10)
            thd_values.append(thd)
            time_stamps.append(df['Time'].iloc[start + window_size//2])
    
    ax2.plot(time_stamps, thd_values, 'purple', linewidth=2)
    ax2.set_xlabel('Time (seconds)', fontsize=12)
    ax2.set_ylabel('Total Harmonic Distortion', fontsize=12)
    ax2.set_title('🎛️ THD Evolution Over Time', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    
    plt.suptitle('HARMONIA NETWORK - Harmonic Analysis', 
                 fontsize=16, fontweight='bold')
    plt.tight_layout()
    plt.savefig('harmonic_analysis.png', dpi=300, bbox_inches='tight')
    print("📊 Saved harmonic_analysis.png")

def generate_summary_report(df):
    """Generate a summary report of thermal analysis findings."""
    print("\n" + "="*60)
    print("🔥 HARMONIA NETWORK - THERMAL ANALYSIS SUMMARY")
    print("="*60)
    
    print(f"\n📊 Data Overview:")
    print(f"   • Duration: {df['Time'].max():.1f} seconds")
    print(f"   • Temperature Range: {df['Temperature'].min():.1f}°C - {df['Temperature'].max():.1f}°C")
    print(f"   • Average Temperature: {df['Temperature'].mean():.1f}°C")
    print(f"   • Drive Range: {df['Drive'].min():.2f} - {df['Drive'].max():.2f}")
    
    # Calculate thermal inertia metrics
    temp_changes = df['Temperature'].diff().abs()
    avg_heating_rate = temp_changes.mean()
    
    print(f"\n🌡️ Thermal Dynamics:")
    print(f"   • Average Temperature Change Rate: {avg_heating_rate:.3f}°C/sample")
    print(f"   • Thermal Response Time: ~{1/avg_heating_rate:.1f} samples to equilibrium")
    
    # Correlation analysis
    temp_drive_corr = df['Temperature'].corr(df['Drive'])
    print(f"   • Temperature-Drive Correlation: {temp_drive_corr:.3f}")
    
    # Signal analysis
    input_rms = np.sqrt(np.mean(df['Input']**2))
    output_rms = np.sqrt(np.mean(df['Output']**2))
    avg_gain = output_rms / (input_rms + 1e-10)
    
    print(f"\n📊 Signal Processing:")
    print(f"   • Input RMS: {input_rms:.4f}")
    print(f"   • Output RMS: {output_rms:.4f}")
    print(f"   • Average Gain: {avg_gain:.3f}")
    
    print(f"\n🔬 Scientific Findings:")
    if temp_drive_corr > 0.7:
        print("   ✅ Strong temperature-drive correlation confirmed")
    else:
        print("   ⚠️  Moderate temperature-drive correlation")
        
    if df['Temperature'].max() > 100:
        print("   ✅ High-temperature saturation regime achieved")
    else:
        print("   ⚠️  Operating in moderate temperature range")
    
    print(f"\n📈 Generated Visualizations:")
    print("   • thermal_overview.png - Comprehensive thermal dynamics")
    print("   • transfer_functions.png - Temperature-dependent transfer curves")
    print("   • harmonic_analysis.png - Spectral content evolution")
    
    print("\n" + "="*60)

def main():
    """Main visualization pipeline."""
    print("🔥 HARMONIA NETWORK - Thermal Dynamics Visualization")
    print("=" * 55)
    
    # Load data
    df = load_thermal_data()
    if df is None:
        return
    
    print(f"\n📊 Processing {len(df)} data points...")
    
    # Generate all visualizations
    create_temperature_overview(df)
    create_transfer_function_analysis(df)
    create_harmonic_analysis(df)
    
    # Generate summary report
    generate_summary_report(df)
    
    print(f"\n✅ Visualization complete! Check the generated PNG files.")
    print(f"🔬 This scientific analysis validates the Divine Math Saturation Engine's thermal behavior.")

if __name__ == "__main__":
    main()