## IBM 1130 simulator customizations:
##
## - Add the Win32 resource file for Windows builds
## - 
import simgen.basic_simulator as SBS

class IBM1130Simulator(SBS.SIMHBasicSimulator):
    '''The IBM650 simulator creates relatively deep stacks, which will fail on Windows.
    Adjust target simulator link flags to provide a 8M stack, similar to Linux.
    '''
    def __init__(self, sim_name, dir_macro, test_name, buildrom):
        super().__init__(sim_name, dir_macro, test_name, buildrom, test_args="-g")

    def write_simulator(self, stream, indent, test_label='ibm650'):
        super().write_simulator(stream, indent, test_label)
        stream.write('\n'.join([
            '',
            'if (WIN32)',
            '    ## Add GUI support, compile in resources:',
            '    target_compile_definitions(ibm1130 PRIVATE GUI_SUPPORT)',
            '    target_sources(ibm1130 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ibm1130.rc)',
            'endif()',
            '',
            '# IBM 1130 utilities:',
            'add_subdirectory(utils)',
            ''
        ]))


