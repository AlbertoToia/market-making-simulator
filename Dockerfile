# Use Node.js 20 on Debian slim for C++ compiler support
FROM node:20-slim

# Install C++ build tools (cmake, g++, make)
RUN apt-get update && apt-get install -y cmake g++ make && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 1. Build C++ Engine
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY tests/ ./tests/
RUN mkdir build && cd build && cmake .. && make -j4

# 2. Build Next.js Frontend
COPY frontend/ ./frontend/
WORKDIR /app/frontend
RUN npm install
RUN npm run build

# 3. Expose port and start Next.js production server
EXPOSE 3000
ENV PORT=3000
ENV NODE_ENV=production

CMD ["npm", "run", "start"]
