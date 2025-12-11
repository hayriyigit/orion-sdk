# Troubleshooting CUDA Kernel Errors

The error you are encountering:
`UserWarning: CUDA error: no kernel image is available for execution on the device`

Indicates that the PyTorch CUDA extensions or kernels were compiled for a different GPU architecture than the one available on your system.

## Diagnosis

1. **Check GPU Compute Capability:**
   Run the following command on the machine where the error occurs (inside the container if possible):
   ```bash
   nvidia-smi --query-gpu=compute_cap --format=csv
   ```
   Or look up your GPU model online. For example, an RTX 3090 is compute capability 8.6.

2. **Check PyTorch Build Configuration:**
   Inside your python environment, run:
   ```python
   import torch
   print(torch.cuda.get_arch_list())
   ```
   This will list the architectures the installed PyTorch supports (e.g., `['sm_37', 'sm_50', 'sm_60', 'sm_70']`).
   If your GPU's capability is not in this list, you will get the "no kernel image" error.

## Solutions

### Option 1: Rebuild Docker Image with Correct Architecture
If you are building the `inference-server` from a Dockerfile:
1. Locate the Dockerfile for `inference-server`.
2. Find where the python dependencies are installed or where the CUDA extensions are compiled.
3. Set the `TORCH_CUDA_ARCH_LIST` environment variable before the build/install steps.
   
   Example in Dockerfile:
   ```dockerfile
   # Replace 8.6 with your GPU's compute capability
   ENV TORCH_CUDA_ARCH_LIST="8.6"
   ENV TORCH_NVCC_FLAGS="-Xfatbin -compress-all"
   
   RUN pip install ...
   ```

### Option 2: Update PyTorch
If you are using a pre-built PyTorch wheel, ensure it matches your CUDA version and GPU generation.
   ```bash
   pip install torch torchvision --index-url https://download.pytorch.org/whl/cu118
   ```
   (Replace `cu118` with the appropriate CUDA version).

### Option 3: Force Recompilation of Extensions
If `efficient_track_anything` compiles custom CUDA kernels (via `setup.py` or JIT compilation):
   ```bash
   export TORCH_CUDA_ARCH_LIST="8.6"
   export FORCE_CUDA=1
   pip install -v --no-cache-dir .
   ```

## Note on Workspace
The file `/app/server/tracker_base/efficient_track_anything/efficienttam_camera_predictor.py` referenced in the logs was not found in the current `orion-sdk` workspace. Please verify if the `inference-server` code is in a separate repository or submodule.
