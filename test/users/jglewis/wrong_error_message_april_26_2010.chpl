module analyze_torus_graphs {

  config const SCALE = 12;

  config param Torus_Base_Index = 1;
  def generate_and_analyze_nD_torus ( const vertex_domain : domain, 
                                      const dense_stencil : domain ) {
    writeln (" why bother!");

  }   


  def main {

    const vertex_domain = [1..10];
    const dense_stencil = [-1..1];

    writeln ():

    generate_and_analyze_nD_torus ( vertex_domain, dense_stencil );

  }

}