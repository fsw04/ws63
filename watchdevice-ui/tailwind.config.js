/** @type {import('tailwindcss').Config} */

export default {
  darkMode: "class",
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    container: {
      center: true,
    },
    extend: {
      colors: {
        'med-bg': '#0A0E1A',
        'med-card': '#111827',
        'med-border': '#1E3A5F',
        'med-green': '#00E5A0',
        'med-red': '#FF4D6A',
        'med-yellow': '#FFB020',
        'med-cyan': '#00D4FF',
        'med-text': '#E0E7FF',
        'med-muted': '#6B7FA3',
        'med-dim': '#A0B4D0',
      },
    },
  },
  plugins: [],
};
