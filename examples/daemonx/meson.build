example_src = files('daemonx.c')
dependencies = [cairo_dep, midi_dep, m_dep]
install = true
if(get_option('usbfix') == true)
    c_args = ['-DTEMP_USB_FIX=1']
endif
