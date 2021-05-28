"""
The Relay Virtual Machine profiler.

Provides extra APIs for profiling vm execution.
"""
# pylint: disable=too-many-instance-attributes
from .. import _ffi
from . import executor


class VirtualMachineProfiler(executor.VirtualMachine):
    """Relay profile VM runtime."""

    def __init__(self, exe, device, enable_cuda_graph=False, cache_interm_tensors=False):
        super(VirtualMachineProfiler, self).__init__(exe, device, enable_cuda_graph)
        self.module = _ffi.vm.VirtualMachineProfiler(exe.module, enable_cuda_graph,
                                                     cache_interm_tensors)
        self._set_devices = self.module["set_devices"]
        self._prepare_context = self.module["prepare_context"]
        self._get_stat = self.module["get_stat"]
        self._get_memory_trace = self.module["get_memory_trace"]
        self._get_interm_tensors = self.module["get_interm_tensors"]
        self._reset = self.module["reset"]
        self._run = self.module["run"]
        self._set_devices(device)

    def get_stat(self, sort_by_time=True, show_shape=True):
        """Get the statistics of executed ops.

        Parameters
        ----------
        sort_by_time: bool
            Set to indicate the returned results are sorted by execution time in
            the descending order. It will be printed in the random order otherwise.

        show_shape: bool
            Whether to display input and output shapes of executed ops. Default True.

        Returns
        -------
        ret : str
            The execution statistics in string.
        """
        return self._get_stat(sort_by_time, show_shape)

    def get_memory_trace(self, show_used=True):
        """Get the memory trace of the execution.

        Parameters
        ----------
        show_used: bool
            Show the total used memory in MBs. If False, then it shows the total allocated
            memory.

        Returns
        -------
        ret: str
            The memory trace that shows the instant memory footprint over op execution.
        """
        return self._get_memory_trace(show_used)

    def get_interm_tensors(self):
        """Get the intermediate results

        Returns
        -------
        ret : Array[Str], Array[Array[Value]], Array[Value]
            op names, op inputs, op outputs
        """
        res = self._get_interm_tensors()
        names, ins, outs = res["names"], res["inputs"], res["outputs"]
        return names, ins, outs

    def reset(self):
        """Reset statistics"""
        self._reset()

    def run(self, *args, func_name="main", profile_memory=False, **kwargs):
        """Run the virtual machine.

        Parameters
        ----------
        args : list[mnm.ndarray] or list[np.ndarray]
            The arguments to the function.

        func_name : str
            The name of function to run.

        profile_memory: bool
            If true, then profile memory footprint without actual execution.
            The result value in this mode becomes a float that indicates
            the total memory footprint in MBs.

        kwargs: dict of str to mnm.ndarray or np.ndarray
            Named arguments to the function.

        Returns
        -------
        result : Union[Object, Dict[str, FloatImm]]
            The output tensors, or memory footprint map in MBs in memory profiling mode.
        """
        # pylint: disable=arguments-differ
        ctx = self.prepare_context(func_name, *args, **kwargs)
        return self._run(ctx, profile_memory)


class VMProfilerExecutor(executor.VMExecutor):
    """
    An implementation of the executor interface for
    the Meta VMProfiler.

    Parameters
    ----------
    mod : :py:class:`~Module`
        The module to support the execution.

    device : str
        The runtime device to run the code on.

    enable_cuda_graph : bool
        Whether to enable cuda graph
    """
    def __init__(self, mod, device, enable_cuda_graph=False, cache_interm_tensors=False):
        super(VMProfilerExecutor, self).__init__(mod, device, enable_cuda_graph)
        self.vm = VirtualMachineProfiler(self.executable, self.device, enable_cuda_graph,
                                         cache_interm_tensors)

    def reset(self):
        """Reset statistics"""
        self.vm.reset()

    def get_stat(self, sort_by_time=True, show_shape=True):
        """Get the statistics of executed ops.

        Parameters
        ----------
        sort_by_time: bool
            Set to indicate the returned results are sorted by execution time in
            the descending order. It will be printed in the random order otherwise.

        show_shape: bool
            Whether to display input and output shapes of executed ops. Default True.

        Returns
        -------
            The execution statistics in string.
        """
        return self.vm.get_stat(sort_by_time, show_shape)

    def get_memory_trace(self, show_used=True):
        """Get the memory trace of the execution.

        Parameters
        ----------
        show_used: bool
            Show the total used memory in MBs. If False, then it shows the total allocated
            memory.

        Returns
        -------
        ret: str
            The memory trace that shows the change of peak memory over op execution.
        """
        return self.vm.get_memory_trace(show_used)

    def get_interm_tensors(self):
        """Get the intermediate results

        Returns
        -------
        ret : Array[Str], Array[Array[Value]], Array[Value]
            op names, op inputs, op outputs
        """
        return self.vm.get_interm_tensors()
