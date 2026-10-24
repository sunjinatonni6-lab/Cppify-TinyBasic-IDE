tailwind.config = {
  theme: {
    extend: {
      fontFamily: {
        sans: ['Inter', 'sans-serif'],
        mono: ['JetBrains Mono', 'monospace']
      },
      colors: {
        cyan: { 400: '#22d3ee', 500: '#06b6d4', 950: '#083344' },
        emerald: { 400: '#34d399', 500: '#10b981', 950: '#022c22' }
      },
      animation: { 'glow-emerald': 'glowEmerald 2s ease-in-out infinite alternate' },
      keyframes: {
        glowEmerald: {
          '0%': { boxShadow: '0 0 5px rgba(16, 185, 129, 0.2)' },
          '100%': { boxShadow: '0 0 20px rgba(16, 185, 129, 0.6)' }
        }
      }
    }
  }
};