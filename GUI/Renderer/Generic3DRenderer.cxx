#include "Generic3DRenderer.h"
#include "SNAPOpenGL.h"
#include "Generic3DModel.h"
#include "GlobalUIModel.h"
#include "SNAPAppearanceSettings.h"
#include "GenericImageData.h"

#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkPolyDataMapper.h"
#include "vtkActor.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSphereSource.h"
#include "vtkCamera.h"
#include "vtkTransform.h"
#include "vtkLineSource.h"
#include "vtkPropAssembly.h"

#include "MeshObject.h"

#include "ColorLabel.h"
#include "IRISApplication.h"
#include "vtkCommand.h"

#include "Window3DPicker.h"

Generic3DRenderer::Generic3DRenderer()
{
  // Why is this necessary?
  GetRenderWindow()->SwapBuffersOff();

  // Initialize the line sources for the axes
  for(int i = 0; i < 3; i++)
    {
    // Create the line source (no point coordinates yet)
    m_AxisLineSource[i] = vtkSmartPointer<vtkLineSource>::New();
    m_AxisLineSource[i]->SetResolution(10);

    // Create mapper, actor, etc
    vtkSmartPointer<vtkPolyDataMapper> mapper =
        vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(m_AxisLineSource[i]->GetOutputPort());

    m_AxisActor[i] = vtkSmartPointer<vtkActor>::New();
    m_AxisActor[i]->SetMapper(mapper);

    this->m_Renderer->AddActor(m_AxisActor[i]);
    }

  // Initialize the mesh assembly
  m_MeshAssembly = vtkSmartPointer<vtkPropAssembly>::New();
  this->m_Renderer->AddActor(m_MeshAssembly);

  // Create a picker
  vtkSmartPointer<Window3DPicker> picker = vtkSmartPointer<Window3DPicker>::New();
  this->GetRenderWindowInteractor()->SetPicker(picker);
}

void Generic3DRenderer::SetModel(Generic3DModel *model)
{
  // Save the model
  m_Model = model;

  // Record and rebroadcast changes in the model
  Rebroadcast(m_Model->GetMesh(), itk::ModifiedEvent(), ModelUpdateEvent());

  // Respond to changes in image dimension - these require big updates
  Rebroadcast(m_Model->GetParentUI()->GetDriver(),
              MainImageDimensionsChangeEvent(), ModelUpdateEvent());

  // Respond to cursor events
  Rebroadcast(m_Model->GetParentUI(), CursorUpdateEvent(), ModelUpdateEvent());

  // Respond to label appearance change events
  Rebroadcast(m_Model->GetParentUI()->GetDriver()->GetColorLabelTable(),
              SegmentationLabelChangeEvent(), ModelUpdateEvent());

  // Update the main components
  this->UpdateAxisRendering();
  this->UpdateCamera(true);

  // Set the model in the picker
  Window3DPicker::SafeDownCast(this->GetRenderWindowInteractor()->GetPicker())->SetModel(m_Model);
}


void Generic3DRenderer::UpdateAxisRendering()
{
  // Update the coordinates of the line source
  IRISApplication *app = m_Model->GetParentUI()->GetDriver();

  if(app->IsMainImageLoaded())
    {
    Vector3ui cursor = app->GetCursorPosition();
    Vector3ui dims = app->GetCurrentImageData()->GetImageRegion().GetSize();

    // Get the axis appearance properties
    SNAPAppearanceSettings *as = m_Model->GetParentUI()->GetAppearanceSettings();
    SNAPAppearanceSettings::Element axisapp =
        as->GetUIElement(SNAPAppearanceSettings::CROSSHAIRS_3D);

    for(int i = 0; i < 3; i++)
      {
      // Update the cursor position
      Vector3d p1 = to_double(cursor), p2 = to_double(cursor);
      p1[i] = 0; p2[i] = dims[i];
      m_AxisLineSource[i]->SetPoint1(p1.data_block());
      m_AxisLineSource[i]->SetPoint2(p2.data_block());
      m_AxisLineSource[i]->Update();

      // Update the visual appearance
      m_AxisActor[i]->GetProperty()->SetColor(1,0,0);
      m_AxisActor[i]->GetProperty()->SetLineWidth(4);

      vtkProperty *prop = m_AxisActor[i]->GetProperty();
      prop->SetColor(axisapp.NormalColor.data_block());
      if(axisapp.DashSpacing > 0)
        {
        prop->SetLineStipplePattern(0x9999);
        prop->SetLineStippleRepeatFactor(static_cast<int>(axisapp.DashSpacing));
        prop->SetLineWidth(axisapp.LineThickness);
        }

      // Update the transform
      vtkSmartPointer<vtkTransform> tran = vtkSmartPointer<vtkTransform>::New();
      tran->SetMatrix(m_Model->GetWorldMatrix().data_block());
      m_AxisActor[i]->SetUserTransform(tran);
      }
    }  
}

void Generic3DRenderer::UpdateCamera(bool reset)
{
  // Update the coordinates of the line source
  IRISApplication *app = m_Model->GetParentUI()->GetDriver();

  if(app->IsMainImageLoaded())
    {
    Vector3ui cursor = app->GetCursorPosition();
    Vector3d spacing = app->GetCurrentImageData()->GetImageSpacing();
    ImageWrapperBase *main = app->GetCurrentImageData()->GetMain();
    Vector3d dim = element_product(to_double(main->GetSize()), spacing);
    Vector3d ctr = main->TransformVoxelIndexToNIFTICoordinates(to_double(cursor));


    if(reset)
      {
      Vector3d x0 = ctr - dim * 0.5, x1 = ctr + dim * 0.5;

      // Center camera on the cursor
      m_Renderer->GetActiveCamera()->SetFocalPoint(ctr[0], ctr[1], ctr[2]);

      // Place camera along the R-L axis
      m_Renderer->GetActiveCamera()->SetPosition(x0[0], ctr[1], ctr[2]);

      // Make camera point so that Superior is up
      m_Renderer->GetActiveCamera()->SetViewUp(0,0,1);

      // Make the camera point at the crosshair. We use the reset camera
      // method, with the bounding box centered on the current cursor position
      m_Renderer->ResetCamera(x0[0], x1[0], x0[1], x1[1], x0[2], x1[2]);
      }
    else
      {
      // Only center the camera, don't change the other view parameters
      m_Renderer->GetActiveCamera()->SetFocalPoint(ctr.data_block());
      }
    }
}

void Generic3DRenderer::paintGL()
{
  // Get the appearance settings
  SNAPAppearanceSettings *as =
      m_Model->GetParentUI()->GetAppearanceSettings();

  // Load the background color
  Vector3d clrBack =
      as->GetUIElement(SNAPAppearanceSettings::BACKGROUND_3D).NormalColor;

  // Set renderer background
  this->m_Renderer->SetBackground(clrBack.data_block());

  // Call the parent's paint method
  AbstractVTKRenderer::paintGL();
}

void Generic3DRenderer::UpdateRendering()
{
  // Get the mesh from the parent object
  MeshObject *mesh = m_Model->GetMesh();

  // Get the app driver
  IRISApplication *driver = m_Model->GetParentUI()->GetDriver();

  // Clear the mesh assembly (and make sure it's modified, in case there are
  // no more meshes to add
  m_MeshAssembly->GetParts()->RemoveAllItems();
  m_MeshAssembly->Modified();

  // For each of the meshes, generate a data mapper and an actor
  for(unsigned int i = 0; i < mesh->GetNumberOfVTKMeshes(); i++)
    {
    // Create a mapper
    vtkSmartPointer<vtkPolyDataMapper> pdm =
        vtkSmartPointer<vtkPolyDataMapper>::New();
    pdm->SetInput(mesh->GetVTKMesh(i));

    // Get the label of that mesh
    const ColorLabel &cl =
        driver->GetColorLabelTable()->GetColorLabel(
          mesh->GetVTKMeshLabel(i));

    // Create a property
    vtkSmartPointer<vtkProperty> prop = vtkSmartPointer<vtkProperty>::New();
    prop->SetColor(cl.GetRGB(0) / 255.0,
                   cl.GetRGB(1) / 255.0,
                   cl.GetRGB(2) / 255.0);
    prop->SetOpacity(cl.GetAlpha() / 255.0);

    // Create an actor
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(pdm);
    actor->SetProperty(prop);

    // Transform the actor from voxel space to physical space
    //vtkSmartPointer<vtkTransform> tran = vtkSmartPointer<vtkTransform>::New();
    //tran->SetMatrix(m_Model->GetWorldMatrix().data_block());
    //actor->SetUserTransform(tran);

    // Add the actor to the renderer
    m_MeshAssembly->AddPart(actor);
    }

}

void Generic3DRenderer::UpdateMeshAppearance()
{
  // Get the mesh from the parent object
  MeshObject *mesh = m_Model->GetMesh();

  // Get the app driver
  IRISApplication *driver = m_Model->GetParentUI()->GetDriver();

  // For each of the meshes, generate a data mapper and an actor
  m_MeshAssembly->GetParts()->InitTraversal();
  for(int i = 0; i < mesh->GetNumberOfVTKMeshes(); i++)
    {
    // Get the next prop
    vtkActor *actor = vtkActor::SafeDownCast(
          m_MeshAssembly->GetParts()->GetNextProp());

    // Get the label of that mesh
    const ColorLabel &cl =
        driver->GetColorLabelTable()->GetColorLabel(
          mesh->GetVTKMeshLabel(i));

    // Get the property
    vtkProperty *prop = actor->GetProperty();

    // Assign the color and opacity
    prop->SetColor(cl.GetRGB(0) / 255.0,
                   cl.GetRGB(1) / 255.0,
                   cl.GetRGB(2) / 255.0);

    if(cl.IsVisibleIn3D())
      prop->SetOpacity(cl.GetAlpha() / 255.0);
    else
      prop->SetOpacity(0.0);
    }

}


void Generic3DRenderer::OnUpdate()
{
  // Update the model first
  m_Model->Update();

  // Deal with the updates to the mesh state
  if(m_EventBucket->HasEvent(itk::ModifiedEvent())
     || m_EventBucket->HasEvent(MainImageDimensionsChangeEvent()))
    {
    this->UpdateRendering();
    }
  else if(m_EventBucket->HasEvent(SegmentationLabelChangeEvent()))
    {
    UpdateMeshAppearance();
    }

  // Deal with camera and axes
  if(m_EventBucket->HasEvent(MainImageDimensionsChangeEvent()))
    {
    UpdateAxisRendering();
    UpdateCamera(true);
    }
  else if(m_EventBucket->HasEvent(CursorUpdateEvent()))
    {
    UpdateAxisRendering();
    UpdateCamera(false);
    }
}

void Generic3DRenderer::ResetView()
{
  this->UpdateCamera(true);
  InvokeEvent(ModelUpdateEvent());
}
