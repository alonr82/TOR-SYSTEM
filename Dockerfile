FROM ubuntu:22.04

# מונע שאלות אוטומטיות בהתקנות
ENV DEBIAN_FRONTEND=noninteractive

# התקנת קומפיילר, ספריות וכלים
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libssl-dev \
    valgrind \
    libsqlcipher-dev \
    && rm -rf /var/lib/apt/lists/*
# הגדרת תיקיית העבודה
WORKDIR /app

# העתקת כל קבצי הפרויקט לתוך הקונטיינר
COPY . .

# 1. קודם כל מקמפלים הכל (כדי ש-make clean לא ימחק לנו דברים חדשים)
RUN make clean && make all

# 2. רק אז יוצרים את תיקיית הקונפיגורציה והקבצים
RUN mkdir -p config && \
    printf "ip=10.0.0.10\nport=9000\n" > dir.cfg && \
    cp dir.cfg config/dir_server_config.cfg