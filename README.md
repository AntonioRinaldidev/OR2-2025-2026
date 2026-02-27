# 🚀 Operations Research 2 - Homeworks

This repository contains the implementation of various algorithms and mathematical models developed for the **Operations Research 2** course (2025/2026). All projects are implemented in **C** with a focus on computational efficiency and memory safety.

Focus of the course is the Travel Salesman Problem, using undirected graphs

---

## 🏛️ Course Information
* **University:** University of Padua (Università degli Studi di Padova)
* **Course:** Operations Research 2 (Ricerca Operativa 2)
* **Students:** Antonio Rinaldi, Adelina Maria Popa
* **Academic Year:** 2025/2026

---

## 📂 Repository Structure

| Directory | Description |
| :--- | :--- |
| `src/` | Source code files (`.c`) |
| `include/` | Header files (`.h`) |
| `data/` | Problem instances and input files (e.g., `.txt`, `.dat`) |
| `docs/` | Mathematical formulations, LaTeX reports, and charts |
| `bin/` | Compiled executables (excluded from Git) |

---

## 🛠️ Build and Usage

### Prerequisites
* **GCC** or **Clang** compiler
* **Make**

### Compilation
The project uses a **Makefile** to automate the build process. To compile, run:
```bash
make
```

### Execution
Run the solver by providing a path to an input instance:
```bash
./bin/solver data/instance_01.txt
```

### Maintenance
To remove compiled files:
```bash
make clean
```

---

## 🔬 Memory Safety
To ensure high-quality code, this project uses **AddressSanitizer**. Memory leaks and invalid accesses are checked automatically during execution. If an error occurs, the program will terminate and provide a detailed debug report.

---

## 📐 Mathematical Framework
Specific models are detailed in the `docs/` folder. General problems follow the standard form:

$$\min \quad z = c^T x$$
$$\text{s.t. } \quad Ax = b, \quad x \ge 0$$

---

## 📈 Status
- [x] Initial repository setup and folder structure.
- [x] Makefile configuration with AddressSanitizer.
- [ ] Implementation of the first homework.