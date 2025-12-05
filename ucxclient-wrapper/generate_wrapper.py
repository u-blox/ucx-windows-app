#!/usr/bin/env python3
"""
UCX API Wrapper Code Generator

This script parses the ucx_api generated headers and automatically creates:
1. C wrapper functions for P/Invoke
2. C# P/Invoke declarations
3. URC callback registrations

Usage:
    python generate_wrapper.py [--output-dir <dir>]
"""

import re
import os
import sys
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, field
from enum import Enum


class ParamType(Enum):
    """Parameter passing type"""
    VALUE = "value"      # Pass by value (int, enum)
    POINTER = "pointer"  # Output pointer (int*, struct*)
    CONST_PTR = "const"  # Input pointer (const char*, const struct*)
    ARRAY = "array"      # Array (uint8_t*, with length param)


@dataclass
class FunctionParam:
    """Represents a function parameter"""
    name: str
    c_type: str
    full_type: str  # With const, *, etc.
    param_type: ParamType
    is_handle: bool = False
    is_output: bool = False
    array_length_param: Optional[str] = None


@dataclass
class Function:
    """Represents a UCX API function"""
    name: str
    return_type: str
    params: List[FunctionParam]
    header_file: str
    module: str  # wifi, bluetooth, etc.
    is_begin: bool = False  # Begin/GetNext pattern
    is_get_next: bool = False
    comment: str = ""


@dataclass
class UrcCallback:
    """Represents a URC callback registration function"""
    register_func: str
    urc_name: str
    module: str
    callback_typedef: str = ""


@dataclass
class TypeDefinition:
    """Represents an enum or struct type"""
    name: str
    kind: str  # 'enum', 'struct', 'typedef'
    definition: str


class UCXHeaderParser:
    """Parses UCX API headers to extract function signatures and types"""
    
    def __init__(self, ucx_api_dir: Path):
        self.ucx_api_dir = ucx_api_dir
        self.functions: List[Function] = []
        self.urc_callbacks: List[UrcCallback] = []
        self.types: List[TypeDefinition] = []
        
        # Regex patterns
        self.func_pattern = re.compile(
            r'^\s*(int32_t|bool|void)\s+([a-zA-Z0-9_]+)\s*\((.*?)\)\s*;',
            re.MULTILINE | re.DOTALL
        )
        self.param_pattern = re.compile(r'\s*([^,]+)')
        self.urc_register_pattern = re.compile(
            r'void\s+(uCx\w+Register\w+)\s*\(\s*uCxHandle_t\s*\*\s*\w+\s*,\s*([^)]+)\s*\)'
        )
        
    def parse_all_headers(self):
        """Parse all header files in the ucx_api directory"""
        header_files = list(self.ucx_api_dir.glob("u_cx_*.h"))
        
        for header_file in header_files:
            if header_file.name in ['u_cx_types.h', 'u_cx_version.h', 'u_cx_urc.h']:
                continue  # Skip these, handle separately
            
            module = header_file.stem.replace('u_cx_', '')
            print(f"Parsing {header_file.name} ({module})...")
            self.parse_header(header_file, module)
    
    def parse_header(self, header_file: Path, module: str):
        """Parse a single header file"""
        content = header_file.read_text()
        
        # Extract functions
        for match in self.func_pattern.finditer(content):
            return_type = match.group(1).strip()
            func_name = match.group(2).strip()
            params_str = match.group(3).strip()
            
            # Skip if not a UCX function
            if not func_name.startswith('uCx'):
                continue
            
            # Parse parameters
            params = self.parse_parameters(params_str)
            
            # Determine if it's Begin/GetNext pattern
            is_begin = 'Begin' in func_name
            is_get_next = 'GetNext' in func_name
            
            func = Function(
                name=func_name,
                return_type=return_type,
                params=params,
                header_file=header_file.name,
                module=module,
                is_begin=is_begin,
                is_get_next=is_get_next
            )
            
            self.functions.append(func)
        
        # Extract URC registrations
        for match in self.urc_register_pattern.finditer(content):
            register_func = match.group(1).strip()
            callback_type = match.group(2).strip()
            
            # Extract URC name from function name
            # e.g., uCxWifiRegisterStationNetworkUp -> StationNetworkUp
            urc_name = register_func.replace('uCx', '').replace('Register', '')
            
            urc = UrcCallback(
                register_func=register_func,
                urc_name=urc_name,
                module=module,
                callback_typedef=callback_type
            )
            
            self.urc_callbacks.append(urc)
    
    def parse_parameters(self, params_str: str) -> List[FunctionParam]:
        """Parse function parameters with improved pointer handling"""
        if not params_str or params_str == 'void':
            return []
        
        params = []
        param_parts = params_str.split(',')
        
        for part in param_parts:
            part = part.strip()
            if not part:
                continue
            
            # Better parsing: separate type from name
            # Handle patterns like "const char *name", "int32_t *pValue", "uStruct_t * pStruct"
            
            # Find the variable name (after the last *)
            # Work backwards from the end
            name_match = re.search(r'(\w+)\s*$', part)
            if not name_match:
                continue
            
            name = name_match.group(1)
            type_part = part[:name_match.start()].strip()
            
            # Count pointers
            pointer_count = type_part.count('*')
            
            # Remove * and whitespace to get base type
            base_type = type_part.replace('*', '').strip()
            
            # Build full_type correctly (e.g., "const char *", "int32_t *")
            if pointer_count > 0:
                full_type = base_type + ' *' * pointer_count
            else:
                full_type = base_type
            
            # Get C type without const
            c_type = base_type.replace('const', '').strip()
            
            # Determine parameter characteristics
            is_const = 'const' in base_type
            is_ptr = pointer_count > 0
            is_handle = 'uCxHandle_t' in c_type or name == 'puCxHandle'
            is_output = is_ptr and not is_const and not is_handle
            
            # Classify parameter type
            if is_handle:
                param_type = ParamType.CONST_PTR
            elif is_const and is_ptr and 'char' in c_type:
                param_type = ParamType.CONST_PTR  # const char* for strings
            elif is_const and is_ptr:
                param_type = ParamType.CONST_PTR  # const input
            elif is_output:
                param_type = ParamType.POINTER  # output parameter
            elif 'uint8_t' in c_type and is_ptr:
                param_type = ParamType.ARRAY
            else:
                param_type = ParamType.VALUE
            
            param = FunctionParam(
                name=name,
                c_type=c_type,
                full_type=full_type.strip(),
                param_type=param_type,
                is_handle=is_handle,
                is_output=is_output
            )
            
            params.append(param)
        
        # Detect array length parameters
        for i, param in enumerate(params):
            if param.param_type == ParamType.ARRAY:
                # Look for length parameter (usually next param with "_len" suffix)
                for j in range(i+1, len(params)):
                    if params[j].name == f"{param.name}_len":
                        param.array_length_param = params[j].name
                        break
        
        return params


class CWrapperGenerator:
    """Generates C wrapper code for P/Invoke"""
    
    def __init__(self, parser: UCXHeaderParser):
        self.parser = parser
    
    def generate_header(self) -> str:
        """Generate C header file - use minimal forward declarations"""
        code = []
        code.append("/* Auto-generated UCX API wrapper header */")
        code.append("/* DO NOT EDIT - Generated by generate_wrapper.py */")
        code.append("/* NOTE: Include ucxclient_wrapper.h instead of this file directly */\n")
        code.append("#ifndef UCXCLIENT_WRAPPER_GENERATED_H")
        code.append("#define UCXCLIENT_WRAPPER_GENERATED_H\n")
        code.append("/* This header is included by ucxclient_wrapper.h which provides all type definitions */")
        code.append("/* Do not include this file directly - it uses opaque forward declarations */\n")
        
        code.append("#ifdef __cplusplus")
        code.append('extern "C" {')
        code.append("#endif\n")
        
        # Note: All function declarations are in the .c file
        # The header just needs to be syntactically correct
        # Real declarations come from including in ucxclient_wrapper.h which has all UCX includes
        
        code.append("/* Function declarations - see ucxclient_wrapper_generated.c */")
        code.append("/* Include ucxclient_wrapper.h to use these functions */\n")
        
        code.append("#ifdef __cplusplus")
        code.append("}")
        code.append("#endif\n")
        code.append("#endif /* UCXCLIENT_WRAPPER_GENERATED_H */")
        
        return '\n'.join(code)
    
    def generate_implementation(self) -> str:
        """Generate C implementation file"""
        code = []
        code.append("/* Auto-generated UCX API wrapper implementation */")
        code.append("/* DO NOT EDIT - Generated by generate_wrapper.py */\n")
        code.append('#include "ucxclient_wrapper_generated.h"')
        code.append('#include "ucxclient_wrapper.h"')
        code.append('#include "../ucxclient/ucx_api/u_cx.h"')
        
        # Include all module headers
        modules = sorted(set(f.module for f in self.parser.functions))
        for module in modules:
            code.append(f'#include "../ucxclient/ucx_api/generated/NORA-W36X/u_cx_{module}.h"')
        
        code.append("")
        
        # Generate function implementations by module
        for module in modules:
            code.append(f"\n/* ========== {module.upper()} FUNCTIONS ========== */\n")
            funcs = [f for f in self.parser.functions if f.module == module]
            
            for func in funcs:
                code.append(self.generate_function_implementation(func))
                code.append("")
        
        return '\n'.join(code)
    
    def generate_function_declaration(self, func: Function) -> str:
        """Generate function declaration"""
        # Map return type
        ret_type = self.map_return_type(func.return_type)
        
        # Generate wrapper function name (use shared method)
        wrapper_name = self.get_wrapper_name(func)
        
        # Generate parameters
        param_list = self.generate_param_list(func, for_declaration=True)
        
        return f"{ret_type} {wrapper_name}({param_list});"
    
    def generate_function_implementation(self, func: Function) -> str:
        """Generate function implementation"""
        ret_type = self.map_return_type(func.return_type)
        wrapper_name = self.get_wrapper_name(func)
        
        param_list = self.generate_param_list(func, for_declaration=True)
        
        code = []
        code.append(f"{ret_type} {wrapper_name}({param_list})")
        code.append("{")
        
        # Instance validation
        code.append("    if (!inst) {")
        code.append("        ucx_wrapper_printf(\"[ERROR] NULL instance in %s\\n\", __func__);")
        code.append(f"        return {self.get_error_return_value(func.return_type)};")
        code.append("    }")
        code.append("")
        
        # Call original function
        call_params = self.generate_call_params(func)
        
        if func.return_type == 'void':
            code.append(f"    {func.name}({call_params});")
            code.append("    return;")
        else:
            code.append(f"    {func.return_type} result = {func.name}({call_params});")
            code.append("    return result;")
        
        code.append("}")
        
        return '\n'.join(code)
    
    def get_wrapper_name(self, func: Function) -> str:
        """Generate consistent wrapper function name"""
        # Remove uCx prefix and module name
        name = func.name
        if name.startswith('uCx'):
            name = name[3:]  # Remove uCx
        
        # Try to remove module prefix (capitalize variations)
        module_caps = func.module.title().replace('_', '')
        if name.startswith(module_caps):
            name = name[len(module_caps):]
        
        # Build wrapper name
        wrapper_name = f"ucx_{func.module}_{name}"
        
        return wrapper_name
    
    def generate_param_list(self, func: Function, for_declaration: bool = True) -> str:
        """Generate parameter list"""
        params = ["ucx_instance_t *inst"]
        
        for param in func.params:
            if param.is_handle:
                continue  # Skip handle, we use inst
            
            params.append(f"{param.full_type} {param.name}")
        
        return ', '.join(params)
    
    def generate_call_params(self, func: Function) -> str:
        """Generate parameters for calling the original function"""
        params = []
        
        for param in func.params:
            if param.is_handle:
                params.append("&inst->cx_handle")
            else:
                params.append(param.name)
        
        return ', '.join(params)
    
    def map_return_type(self, c_type: str) -> str:
        """Map C return type to wrapper return type"""
        return c_type  # Keep same for now
    
    def get_error_return_value(self, return_type: str) -> str:
        """Get error return value for a given type"""
        if return_type == 'void':
            return ''
        elif return_type == 'bool':
            return 'false'
        elif return_type in ['int32_t', 'int']:
            return '-1'
        else:
            return '0'


class CSharpPInvokeGenerator:
    """Generates C# P/Invoke declarations"""
    
    def __init__(self, parser: UCXHeaderParser):
        self.parser = parser
    
    def generate(self) -> str:
        """Generate C# P/Invoke file"""
        code = []
        code.append("// Auto-generated UCX API wrapper P/Invoke declarations")
        code.append("// DO NOT EDIT - Generated by generate_wrapper.py\n")
        code.append("using System;")
        code.append("using System.Runtime.InteropServices;")
        code.append("using System.Text;\n")
        code.append("namespace UcxAvaloniaApp.Services")
        code.append("{")
        code.append("    public static partial class UcxNativeGenerated")
        code.append("    {")
        code.append('        private const string DllName = "ucxclient_wrapper";\n')
        
        # Generate by module
        modules = sorted(set(f.module for f in self.parser.functions))
        
        for module in modules:
            code.append(f"        #region {module.upper()} Functions\n")
            funcs = [f for f in self.parser.functions if f.module == module]
            
            for func in funcs:  # Generate ALL functions
                code.append(self.generate_pinvoke_declaration(func))
                code.append("")
            
            code.append("        #endregion\n")
        
        code.append("    }")
        code.append("}")
        
        return '\n'.join(code)
    
    def get_wrapper_name(self, func: Function) -> str:
        """Generate consistent wrapper function name"""
        # Remove uCx prefix and module name
        name = func.name
        if name.startswith('uCx'):
            name = name[3:]  # Remove uCx
        
        # Try to remove module prefix (capitalize variations)
        module_caps = func.module.title().replace('_', '')
        if name.startswith(module_caps):
            name = name[len(module_caps):]
        
        # Build wrapper name
        wrapper_name = f"ucx_{func.module}_{name}"
        
        return wrapper_name
    
    def generate_pinvoke_declaration(self, func: Function) -> str:
        """Generate P/Invoke declaration with proper marshaling"""
        wrapper_name = self.get_wrapper_name(func)
        
        # Map return type
        cs_return_type = self.map_csharp_type(func.return_type, is_return=True)
        
        # Generate parameters
        params = ["IntPtr instance"]
        marshal_attrs = []
        
        for param in func.params:
            if param.is_handle:
                continue
            
            # Determine C# type and marshaling
            if param.param_type == ParamType.CONST_PTR and 'char' in param.c_type:
                # const char* -> string
                params.append(f"[MarshalAs(UnmanagedType.LPStr)] string {param.name}")
            elif param.param_type == ParamType.ARRAY:
                # byte array
                params.append(f"byte[] {param.name}")
                if param.array_length_param:
                    # Skip length param, it's already in the array
                    pass
            elif param.param_type == ParamType.POINTER:
                # Output parameter
                cs_type = self.map_csharp_type(param.c_type, is_pointer=False)
                params.append(f"out {cs_type} {param.name}")
            elif param.param_type == ParamType.VALUE:
                # Value parameter
                cs_type = self.map_csharp_type(param.c_type, is_pointer=False)
                # Check if it's an array length parameter that we should skip
                is_array_len = False
                for other_param in func.params:
                    if other_param.array_length_param == param.name:
                        is_array_len = True
                        break
                if not is_array_len:
                    params.append(f"{cs_type} {param.name}")
            else:
                # Generic pointer (struct, etc.)
                if 'char' in param.c_type:
                    params.append(f"StringBuilder {param.name}")
                else:
                    params.append(f"IntPtr {param.name}")
        
        param_str = ', '.join(params)
        
        code = []
        code.append("        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]")
        code.append(f"        public static extern {cs_return_type} {wrapper_name}({param_str});")
        
        return '\n'.join(code)
    
    def map_csharp_type(self, c_type: str, is_return: bool = False, is_pointer: bool = False) -> str:
        """Map C type to C# type"""
        type_map = {
            'int32_t': 'int',
            'uint32_t': 'uint',
            'int16_t': 'short',
            'uint16_t': 'ushort',
            'int8_t': 'sbyte',
            'uint8_t': 'byte',
            'bool': 'bool',
            'void': 'void',
            'char': 'byte',
        }
        
        base_type = c_type.replace('const', '').replace('*', '').strip()
        
        if base_type in type_map:
            cs_type = type_map[base_type]
        elif 'char' in base_type and is_pointer:
            return 'string' if 'const' in c_type else 'StringBuilder'
        else:
            cs_type = base_type
        
        if is_pointer and cs_type not in ['string', 'StringBuilder']:
            if is_return:
                return f"IntPtr"
            else:
                return f"ref {cs_type}"
        
        return cs_type


def main():
    """Main entry point"""
    script_dir = Path(__file__).parent
    ucx_api_dir = script_dir.parent / "ucxclient" / "ucx_api" / "generated" / "NORA-W36X"
    
    if not ucx_api_dir.exists():
        print(f"ERROR: UCX API directory not found: {ucx_api_dir}")
        return 1
    
    print("=" * 60)
    print("UCX API Wrapper Code Generator")
    print("=" * 60)
    print(f"UCX API directory: {ucx_api_dir}\n")
    
    # Parse headers
    parser = UCXHeaderParser(ucx_api_dir)
    parser.parse_all_headers()
    
    print(f"\n Found {len(parser.functions)} functions")
    print(f"Found {len(parser.urc_callbacks)} URC callbacks\n")
    
    # Generate C wrapper
    print("Generating C wrapper...")
    c_gen = CWrapperGenerator(parser)
    header_code = c_gen.generate_header()
    impl_code = c_gen.generate_implementation()
    
    header_file = script_dir / "ucxclient_wrapper_generated.h"
    impl_file = script_dir / "ucxclient_wrapper_generated.c"
    
    header_file.write_text(header_code)
    impl_file.write_text(impl_code)
    print(f"  Generated: {header_file.name}")
    print(f"  Generated: {impl_file.name}")
    
    # Generate C# P/Invoke
    print("\nGenerating C# P/Invoke declarations...")
    cs_gen = CSharpPInvokeGenerator(parser)
    cs_code = cs_gen.generate()
    
    cs_file = script_dir.parent / "ucx-avalonia-app" / "Services" / "UcxNativeGenerated.cs"
    cs_file.write_text(cs_code)
    print(f"  Generated: {cs_file.name}")
    
    print("\n" + "=" * 60)
    print("Code generation complete!")
    print("=" * 60)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
