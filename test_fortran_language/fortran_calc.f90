module calc_module
    implicit none
    contains
    
    function add_numbers(a, b) result(sum) bind(C, name="fortran_add")
        use iso_c_binding
        integer(c_int), value :: a, b
        integer(c_int) :: sum
        sum = a + b
    end function add_numbers
    
    function multiply_numbers(a, b) result(product) bind(C, name="fortran_multiply")
        use iso_c_binding
        integer(c_int), value :: a, b
        integer(c_int) :: product
        product = a * b
    end function multiply_numbers
    
end module calc_module